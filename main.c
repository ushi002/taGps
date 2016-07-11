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
#include "butif.h"

#define GPSRXCHAR	0x01
#define GPS_PULSE	0x02
#define CHECKACK	0x04
#define PC_UART_RX	0x08
#define BUTTON1		0x10
#define WTDOG		0x20

typedef enum ButtonStep_t {
	buttonStep_poll_cfg = 0,
	buttonStep_set_cfg = 1,
	buttonStep_poll_pvt = 2,
	buttonStep_end = 3
}ButtonStep_e;
//#pragma PERSISTENT(memGpsStatMap)
//32768 pages 64MBit ADESTO memory
//U8 memGpsStatMap[32786] __attribute__((section(".TI.persistent")));
//#pragma PERSISTENT(cmdToDo)
static U16 cmdToDo = 0;
static Boolean gGpsInitialized = false;

static Boolean gGpsPowered = false;
static Boolean gUsbPowered = false;

//GPS time pulses to store GPS position
static U16 gps_time_pulse_secs = 1;
//number of generated GPS time pulses
static U16 gps_time_pulse_num = 0;

static void init_configure_gps(void);

int main(void)
{
	const Message_s * ubxmsg;

	U16 blinkRedPwr2 = 0;

	//WDOG interrupt mode
	WDTCTL = WDTPW | WDTSSEL__VLO | WDTTMSEL | WDTCNTCL | WDTIS__32K;
	SFRIE1 |= WDTIE;                          // Enable WDT interrupt

	//default initialization:
	//initialize all ports to eliminate current wasting
	P1DIR = 0xFF;
	P1OUT = 0x00;
	P2DIR = 0xFF;
	P2OUT = 0x00;
	P3DIR = 0xFF;
	P3OUT = 0x00;
	P4DIR = 0xFF;
	P4OUT = 0x00;
	P5DIR = 0xFF;
	P5OUT = 0x00;
	P6DIR = 0xFF;
	P6OUT = 0x00;
	P7DIR = 0xFF;
	P7OUT = 0x00;
	P8DIR = 0xFF;
	P8OUT = 0x00;
	P9DIR = 0xFF;
	P9OUT = 0x00;
	//this port does not exist but satisfy TI compiler..:
	P10DIR = 0xFF;
	P10OUT = 0x00;

	//must be before other input interrupts configuration:
	but_init();

	dbg_initport();
	gps_initport();
	led_initport();

	// Disable the GPIO power-on default high-impedance mode to activate
	// previously configured port settings
	PM5CTL0 &= ~LOCKLPM5;

	// Startup clock system with max DCO setting ~8MHz
	CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
	CSCTL1 = DCOFSEL_3 | DCORSEL;             // Set DCO to 8MHz
	CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
	CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers
	CSCTL0_H = 0;                             // Lock CS registers

	//configure TIMERs_A
	TA0CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA0CCR0 = 50000;
	TA0CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet
	TA1CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA1CCR0 = 50000;
	TA1CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet

	dbg_inituart();
	gps_inituart();
	spi_init();
	ubx_init();

	dbg_txmsg("\nWelcome to taGPS program\n");

	dbg_txmsg("\nEnter the operational mode and sleep...");

	while (1)
	{

		if (!gps_txempty() && !(UCA1STATW & UCBUSY))
		{
			//send a character to GPS
			UCA1TXBUF = gps_txbpop();
		}

		if (!buff_empty() && !(UCA0STATW & UCBUSY))
		{
			//send an SPI FLASH character to PC UART
			UCA0TXBUF = buff_pop();
		}

		if (!spi_txempty() && !(UCB0STATW & UCBUSY))
		{
			//send a character to FLASH
			UCB0TXBUF = spi_txchpop();
		}

		if (!cmdToDo)
		{
			//nothing to do, fall asleep
			__bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, interrupts enabled
			__no_operation();                       // For debugger
		}

		if (blinkRedPwr2 > 0)
		{
			cmdToDo &= ~WTDOG;
			blinkRedPwr2--;
			led_swap_red();

			if (blinkRedPwr2 == 0)
			{
				//return to original configuration:
				SFRIE1 &= ~WDTIE;
				WDTCTL = WDTPW | WDTSSEL__VLO | WDTTMSEL | WDTCNTCL | WDTIS__32K;
				SFRIFG1 &= ~WDTIFG;
				SFRIE1 |= WDTIE;
				led_off();

				//enable interrupt egain
				P2IFG = 0;		// Clear all P2 interrupt flags
				P2IE |= BIT0;   //enable again interrupts
			}

		}

		if (cmdToDo & WTDOG)
		{
			cmdToDo &= ~WTDOG;
			//regular checks...
			if (pcif_has_power() && !gUsbPowered)
			{
				//enable PC communication
				pcif_enif();
				gUsbPowered = true;
				//USB will read/write SPI
				spi_enrx();
			}
			if (!pcif_has_power() && gUsbPowered)
			{
				//disable PC communication
				pcif_disif();
				gUsbPowered = false;
				//without USB no reads from the SPI memory
				spi_disrx();
			}
			//is GPS powered?
			if (gps_has_power() && !gGpsPowered)
			{
				gGpsPowered = true;
				gps_uart_enable();
				if (!gGpsInitialized)
				{
					init_configure_gps();
					gGpsInitialized = true;
					gps_ie(); //enable interrupts
					//blick green that we have configured GPS chip
					led_on_green();
					TA0CTL = TASSEL__SMCLK | ID__8 | MC__UP;        // SMCLK, start timer
					TA0EX0 = TAIDEX_7;
				}
			}
			if (!gps_has_power() && gGpsPowered)
			{
				gGpsPowered = false;
				gps_id(); //disable interrupt
				gps_uart_disable();
				gGpsInitialized = false;
				//GPS is turned off
				//blick green that we received gps pulse
				led_on_red();
				TA1CTL = TASSEL__SMCLK | ID__8 | MC__UP;        // SMCLK, start timer
				TA1EX0 = TAIDEX_7;
			}
		}

		if (!gGpsPowered && !gGpsInitialized)
		{
			//configure GPS:
			if (cmdToDo & BUTTON1)
			{
				//konfigure for led blinking
				cmdToDo &= ~BUTTON1;
				SFRIE1 &= ~WDTIE;
				WDTCTL = WDTPW | WDTSSEL__VLO | WDTTMSEL | WDTCNTCL | WDTIS__512;
				SFRIE1 |= WDTIE;

				gps_time_pulse_secs++;
				if (gps_time_pulse_secs > 5)
				{
					gps_time_pulse_secs = 1;
				}
				blinkRedPwr2 = gps_time_pulse_secs << 1;
			}
		}

		if (gGpsPowered && gGpsInitialized)
		{
			if (cmdToDo & BUTTON1)
			{
				cmdToDo &= ~BUTTON1;
			}

			if (cmdToDo & GPS_PULSE)
			{
				gps_time_pulse_num++;
				if (gps_time_pulse_num >= gps_time_pulse_secs)
				{
					//blick green that we received gps pulse
					led_on_green();
					TA0CTL = TASSEL__SMCLK | ID__1 | MC__UP;        // SMCLK, start timer

					gps_time_pulse_num = 0;
					cmdToDo &= ~GPS_PULSE;
					dbg_txmsg("\nPoll GPS status");
					ubxmsg = ubx_get_msg(MessageIdPollPvt);
					gps_cmdtx(ubxmsg->pMsgBuff);
				}
			}

			if (cmdToDo & GPSRXCHAR)
			{
				cmdToDo &= ~GPSRXCHAR;
				gps_rx_ubx_msg(ubxmsg, true);
				if (ubxmsg->confirmed)
				{
					dbg_txmsg("\nConfirmed! ");
					spiif_storeubx(ubxmsg);
					//displej_this(ubxmsg->pBody->navPvt.year);
					ubx_get_msg(MessageIdPollPvt);
				}
			}
		}

		if (cmdToDo & PC_UART_RX)
		{
			cmdToDo &= ~PC_UART_RX;
			pcif_rxchar();
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
	//set GPS PULSE artifficialy
	//P2IFG |= BIT1;
	cmdToDo |= WTDOG;
	__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
}

// Timer0_A0 for green led interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer0_A0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER0_A0_VECTOR))) Timer0_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	TA0CTL = MC__STOP;        // SMCLK, do not count yet
	TA0EX0 = TAIDEX_0; 		  //clear extended divider
	led_off_green();
}

// Timer1_A0 for red led interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER1_A0_VECTOR
__interrupt void Timer1_A0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER1_A0_VECTOR))) Timer1_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	TA1CTL = MC__STOP;        // SMCLK, do not count yet
	TA1EX0 = TAIDEX_0; 		  //clear extended divider
	led_off_red();
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
		cmdToDo |= PC_UART_RX;
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
		break;
	case USCI_UART_UCTXIFG:
		if (!buff_empty())
		{
			UCA0TXBUF = buff_pop();
		}
		break;
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
		cmdToDo |= GPSRXCHAR;
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
		break;
	case USCI_UART_UCTXIFG:
		if (!gps_txempty())
		{
			UCA1TXBUF = gps_txbpop();
		}
		break;
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
	switch(__even_in_range(P2IV, P2IV_P2IFG7))
	{
	case P2IV_NONE: break;
	case P2IV_P2IFG0:
		cmdToDo |= BUTTON1;
		//?? musi? P2IE &= ~BIT0;  //disable following interrupts
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
		break;
	case P2IV_P2IFG1:
		//GPS TIME PULSE:
		cmdToDo |= GPS_PULSE;
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
		//23*4 bajtu je NAV message...
		//264 bajtu/stranka
		//mame 32768 stranek - 9hodin->stranka/zaznam/sekunda
		break;
	case P2IV_P2IFG2: break;
	case P2IV_P2IFG3: break;
	case P2IV_P2IFG4: break;
	case P2IV_P2IFG5: break;
	case P2IV_P2IFG6: break;
	case P2IV_P2IFG7: break;
	}
}

//SPI INTERRUPT ROUTINE:
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_B0_VECTOR))) USCI_B0_ISR (void)
#else
#error Compiler not supported!
#endif
{
  switch(__even_in_range(UCB0IV, USCI_SPI_UCTXIFG))
  {
    case USCI_NONE: break;
    case USCI_SPI_UCRXIFG:
      spi_rxchpush();
      break;
    case USCI_SPI_UCTXIFG:
		if (!spi_txempty())
		{
			UCB0TXBUF = spi_txchpop();
		}
      break;
    default: break;
  }
}

static void init_configure_gps(void)
{
	const Message_s * ubxmsg;
	U16 rx_msg_res;
	U16 init_cfg_try_num;
	Boolean init_seq = true;

	//reinitialize GPS UART
	//gps_inituart();

	dbg_txmsg("\nPoll UBX CFG PORT message:\n");
	//prepare port cfg msg
	ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
	//send it
	gps_initcmdtx(ubxmsg->pMsgBuff);
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
			gps_initcmdtx(ubxmsg->pMsgBuff);
		}
	}

	dbg_txmsg("\n\nTurning off NMEA GPS port... ");
	while(1)
	{
		ubxmsg = ubx_get_msg(MessageIdSetCfgPrt);
		gps_initcmdtx(ubxmsg->pMsgBuff);
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
