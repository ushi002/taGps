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
#include "spiif.h"
#include "ubxprot.h"
#include "ledif.h"

#define GPSRXCHAR	0x01
#define BUTTON1		0x02
#define CHECKACK	0x04

typedef enum ButtonStep_t {
	buttonStep_poll_cfg = 0,
	buttonStep_set_cfg = 1,
	buttonStep_poll_pvt = 2,
	buttonStep_end = 3
}ButtonStep_e;

static U16 cmdToDo = 0;
static Boolean gGpsInitialized = false;

static void init_configure_gps(void);

int main(void)
{
	const Message_s * ubxmsg;

	//WDOG interrupt mode
	WDTCTL = WDTPW | WDTSSEL__SMCLK | WDTTMSEL | WDTCNTCL | WDTIS__8192K;
	SFRIE1 |= WDTIE;                          // Enable WDT interrupt

	dbg_initport();
	gps_initport();
	led_initport();

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
	spi_init();
	ubx_init();

	dbg_txmsg("\nWelcome to taGPS program\n");

	dbg_txmsg("\nInitialization done, let us sleep...");

	__bis_SR_register(LPM3_bits | GIE);       // Enter LPM3, interrupts enabled
	__no_operation();                         // For debugger
	while (1)
	{
		//go sleep
		__bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, interrupts enabled
		__no_operation();                       // For debugger

		//is GPS powered?
		if (gps_has_power())
		{
			led_ok();
			if (!gGpsInitialized)
			{
				init_configure_gps();
				gGpsInitialized = true;
				gps_uart_ie(); //enable interrupt
			}
		}else
		{
			//GPS is turned off
			led_error();
			gGpsInitialized = false;
			gps_uart_id(); //disable interrupt
		}

		if (cmdToDo & BUTTON1)
		{
			cmdToDo &= ~BUTTON1;
			dbg_txmsg("\nPoll GPS status");
			ubxmsg = ubx_get_msg(MessageIdPollPvt);
			gps_cmdtx(ubxmsg->pMsgBuff);
		}

		if (cmdToDo & GPSRXCHAR)
		{
			cmdToDo &= ~GPSRXCHAR;
			gps_rx_ubx_msg(ubxmsg, true);
			if (ubxmsg->confirmed)
			{
				dbg_txmsg("\nConfirmed! ");
				//displej_this(ubxmsg->pBody->navPvt.year);
			}
		}
	}
}

// Watchdog Timer interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) WDT_ISR (void)
#else
#error Compiler not supported!
#endif
{
	__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
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
		//UCA1IE &= ~UCRXIE; //disable interrupt
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
	P2IFG &= ~BIT0;                           // Clear P2.0 IFG
	cmdToDo |= BUTTON1;
    __bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
    __no_operation();
}


static void init_configure_gps(void)
{
	const Message_s * ubxmsg;
	U16 rx_msg_res;
	U16 init_cfg_try_num;
	Boolean init_seq = true;

	dbg_txmsg("\nPoll UBX CFG PORT message:\n");
	//prepare port cfg msg
	ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
	//send it
	gps_cmdtx(ubxmsg->pMsgBuff);
	//wait for answer:
	init_cfg_try_num = 0;
	while(init_seq)
	{
		init_cfg_try_num++;
		rx_msg_res = gps_rx_ubx_msg(ubxmsg, false);
		switch(rx_msg_res)
		{
		case 3:
			dbg_txmsg("\nUBX message too long!");
			break;
		case 4:
			dbg_txmsg("\nUBX message CRC Error!");
			break;
		case 5:
			if (ubxmsg->confirmed)
			{
				init_seq = false;
				dbg_txmsg("\nUBX message confirmed.");
				init_cfg_try_num = 0;
			}
			break;
		case 0:
			//waiting for next character
			break;
		default:
			dbg_txmsg("\nGPS unknown ERROR!");
			break;

		}

		if (init_cfg_try_num > (USHRT_MAX>>2))
		{
			led_error();
			dbg_txmsg("\nInit UBX message error! Send again poll cfg...");
			init_cfg_try_num = 0;
			ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
			gps_cmdtx(ubxmsg->pMsgBuff);
		}
	}

	dbg_txmsg("\n\nTurning off NMEA GPS port... ");
	while(1)
	{
		ubxmsg = ubx_get_msg(MessageIdSetCfgPrt);
		gps_cmdtx(ubxmsg->pMsgBuff);
		while(1)
		{
			rx_msg_res = gps_rx_ubx_msg(ubxmsg, false);
			if (rx_msg_res != 0)
			{
				//message received
				break;
			}else
			{
				init_cfg_try_num++;
				if (init_cfg_try_num > (USHRT_MAX>>2))
				{
					//try again
					init_cfg_try_num = 0;
					break;
				}
			}

		}
		if (!ubxmsg->confirmed)
		{
			dbg_txmsg("\n\nMessage not confirmed, trying again... ");
		}else
		{
			break;
		}
	}
	dbg_txmsg("...NMEA GPS port is off.");
	led_ok();
}
