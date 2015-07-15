//******************************************************************************
//  MSP430FR69xx Demo - eUSCI_A0 UART echo at 9600 baud using BRCLK = 8MHz
//
//  Description: This demo echoes back characters received via a PC serial port.
//  SMCLK/ DCO is used as a clock source and the device is put in LPM3
//  The auto-clock enable feature is used by the eUSCI and SMCLK is turned off
//  when the UART is idle and turned on when a receive edge is detected.
//  Note that level shifter hardware is needed to shift between RS232 and MSP
//  voltage levels.
//
//  The example code shows proper initialization of registers
//  and interrupts to receive and transmit data.
//  To test code in LPM3, disconnect the debugger.
//
//  ACLK = VLO, MCLK =  DCO = SMCLK = 8MHz
//
//                MSP430FR6989
//             -----------------
//       RST -|     P4.2/UCA0TXD|----> PC (echo)
//            |                 |
//            |                 |
//            |     P4.3/UCA0RXD|<---- PC
//            |                 |
//
//   William Goh
//   Texas Instruments Inc.
//   April 2014
//   Built with IAR Embedded Workbench V5.60 & Code Composer Studio V6.0
//******************************************************************************
#include <msp430.h>
#include "typedefs.h"
#include "gpsif.h"
#include "dbgif.h"
#include "ubxprot.h"

#define GPSRXCHAR	0x01
#define BUTTON1		0x02
#define CHECKACK	0x04

typedef enum ButtonStep_t {
	buttonStep_poll_cfg = 0,
	buttonStep_set_cfg = 1,
	buttonStep_end = 2
}ButtonStep_e;

static U16 cmdToDo = 0;

static ButtonStep_e button_step = buttonStep_poll_cfg;

int main(void)
{
	const Message_s * ubxmsg;
	U16 rx_res;

	WDTCTL = WDTPW | WDTHOLD;                 // Stop Watchdog

	dbg_initport();
	gps_initport();

	P6DIR |= BIT5 | BIT6;                     // Set P6.0 to output direction
	P6OUT  = BIT5 | BIT6;

	P2DIR = 0xFF ^ BIT0;                      // Set all but P2.0 to output direction
	P2REN = BIT0;                             // Pull resistor enable for P2.0
	P2OUT = 0;                                // Pull-down resistor on P2.0

	P2IES = 0;                                // P2.0 Lo/Hi edge
	P2IE = BIT0;                              // P2.0 interrupt enable
	P2IFG = 0;                                // Clear all P2 interrupt flags

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	// Startup clock system with max DCO setting ~8MHz
	CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
	CSCTL1 = DCOFSEL_3 | DCORSEL;             // Set DCO to 8MHz
	CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers
	CSCTL0_H = 0;                             // Lock CS registers

	dbg_inituart();
	gps_inituart();
	ubx_init();

	__bis_SR_register(LPM3_bits | GIE);       // Enter LPM3, interrupts enabled
	__no_operation();                         // For debugger
	while (1)
	{

		if(cmdToDo & BUTTON1)
		{
			dbg_ledsoff();
			switch (button_step)
			{
			case buttonStep_poll_cfg:
				ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
				dbg_ledok();
				break;
			case buttonStep_set_cfg:
				ubxmsg = ubx_get_msg(MessageIdSetCfgPrt);
				dbg_lederror();
				break;
			default:
				break;
			}

			cmdToDo &= ~BUTTON1;
			//cmdToDo |= W4ACKMSG;						   //wait for answer
			P2IE   = BIT0;                              // P2.0 interrupt enable
			P2IFG &= ~BIT0;                           // Clear P2.0 IFG
			gps_cmdtx(ubxmsg->pMsgBuff);
			gps_uart_ie();
		}

		if(cmdToDo & GPSRXCHAR)
		{
			rx_res = gps_rxchar(ubxmsg);

			if (rx_res)
			{
				//not expecting following message characters, stop listening
				gps_uart_id();
			}

			switch (rx_res)
			{
			case 0:
				//character received, sleep until next comes
				cmdToDo &= ~GPSRXCHAR;
				break;
			case 1:
				//not an ubx message, quit
				cmdToDo &= ~GPSRXCHAR;
				break;
			case 2:
				//need one more loop to process the message
				break;
			case 3:
				//error, message too long
				cmdToDo &= ~GPSRXCHAR;
				break;
			case 4:
				//wrong message checksum, start over
				cmdToDo &= ~GPSRXCHAR;
				break;
			case 5:
				//packet received succesfully, check message confirmation
				cmdToDo |= CHECKACK;
				cmdToDo &= ~GPSRXCHAR;
				break;
			default:
				//unknown, start over
				cmdToDo &= ~GPSRXCHAR;
				break;
			}
		}

		if(cmdToDo & CHECKACK)
		{
			if (ubxmsg->confirmed == true)
			{
				//move to next step
				button_step++;
				if (button_step == buttonStep_end)
				{
					//start again
					button_step = buttonStep_poll_cfg;
				}
			}
			cmdToDo &= ~CHECKACK;
		}


		if (!cmdToDo)
		{
			//go sleep
			__bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, interrupts enabled
			__no_operation();                       // For debugger
		}
	}
}

//USCI_A0 interrupt routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A0_VECTOR
__interrupt void USCI_A0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A0_VECTOR))) USCI_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(UCA0IV, USCI_UART_UCTXCPTIFG))
	{
	case USCI_NONE: break;
	case USCI_UART_UCRXIFG:
		P6OUT ^= BIT5 | BIT6;                      // Toggle LEDs
		while(!(UCA0IFG&UCTXIFG));
		UCA0TXBUF = UCA0RXBUF;
		__no_operation();
		break;
	case USCI_UART_UCTXIFG: break;
	case USCI_UART_UCSTTIFG: break;
	case USCI_UART_UCTXCPTIFG: break;
	}
}

//USCI_A1 interrupt routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(UCA1IV, USCI_UART_UCTXCPTIFG))
	{
	case USCI_NONE: break;
	case USCI_UART_UCRXIFG:
		//receving character
		//P6OUT ^= BIT5 | BIT6;                      // Toggle LEDs
		cmdToDo |= GPSRXCHAR;
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
//		while(!(UCA0IFG&UCTXIFG));
//		UCA0TXBUF = UCA1RXBUF;
//		__no_operation();
		break;
	case USCI_UART_UCTXIFG: break;
	case USCI_UART_UCSTTIFG: break;
	case USCI_UART_UCTXCPTIFG: break;
	}
}

// Port 2 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT2_VECTOR
__interrupt void Port_2(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT2_VECTOR))) Port_2 (void)
#else
#error Compiler not supported!
#endif
{
	P2IE = 0;                              // P2.0 interrupt disable
	P2IFG &= ~BIT0;                           // Clear P2.0 IFG
    cmdToDo |= BUTTON1;
    __bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
    __no_operation();
}
