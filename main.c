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
			UCA1TXBUF = gps_txbpop();
		}

		if (!buff_empty() && !(UCA0STATW & UCBUSY))
		{
			UCA0TXBUF = buff_pop();
		}

		if (!spi_txempty() && !(UCB0STATW & UCBUSY))
		{
			UCB0TXBUF = spi_txchpop();
		}

		//fall asleep
		__bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, interrupts enabled
		__no_operation();                       // For debugger

		if (blinkRedPwr2 > 0)
		{
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
				P2IE |= BIT0;  //disable following interrupts
			}

		}

		//is GPS powered?
		if (gps_has_power())
		{
			//disable PC communication
			pcif_disif();
			if (!gGpsInitialized)
			{
				//here we only write to the SPI
				spi_disrx();
				init_configure_gps();
				gGpsInitialized = true;
				led_ok();
				gps_uart_ie(); //enable interrupt
			}
		}else
		{
			//enable PC communication
			pcif_enif();
			if (gGpsInitialized)
			{
				//we will read/write SPI
				spi_enrx();
				//GPS is turned off
				led_off();
				gGpsInitialized = false;
				gps_uart_id(); //disable interrupt
			}
			//check buttons
			//but_check();

			if (cmdToDo & BUTTON1)
			{
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

		if (cmdToDo & GPS_PULSE)
		{
			cmdToDo &= ~GPS_PULSE;
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
				spiif_storeubx(ubxmsg);
				//displej_this(ubxmsg->pBody->navPvt.year);
				ubx_get_msg(MessageIdPollPvt);
			}
		}

		if (cmdToDo & PC_UART_RX)
		{
			cmdToDo &= ~PC_UART_RX;
			pcif_rxchar();
		}

		//clear "one loop" commands:
		cmdToDo &= ~BUTTON1;
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
	__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
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
		P2IE &= ~BIT0;  //disable following interrupts
		__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
		__no_operation();
		break;
	case P2IV_P2IFG1:
		//GPS TIME PULSE:
		gps_time_pulse_num++;

		if (gps_time_pulse_secs >= gps_time_pulse_num)
		{
			gps_time_pulse_num = 0;
			led_swap_green();
			cmdToDo |= GPS_PULSE;
			__bic_SR_register_on_exit(LPM3_bits);     // Exit LPM3
			__no_operation();
		}
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
	gps_inituart();

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
