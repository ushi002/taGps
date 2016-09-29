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

//#define ARTIFICIAL_GPS
#ifdef ARTIFICIAL_GPS
U16 gGpsDiv = 0;
#endif //ARTIFICIAL_GPS

#define GPSRXCHAR	0x01
#define GPS_PULSE	0x02
#define CHECKACK	0x04
#define PC_UART_RX	0x08
#define BUTTON1		0x10
#define NXTGPSCMD   0x20

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
static Boolean gGpsRestarted = false;

static Boolean gGpsPowerChange = false;
static Boolean gGpsPowered = false;
static Boolean gUsbPowerChange = false;
static Boolean gUsbPowered = false;
static Boolean gpower_save_mode_activated = false;
//static Boolean gContinue = false;
static U16 gBlinkRed = 0;

volatile U16 gAdcBatteryVal = 0;

//GPS time pulses to store GPS position
static const U16 gps_pulse_cfg_ar[] = {1, 5, 20};
static const U16 gps_pulse_cfg_option_num = 3;
static U16 gps_time_pulse_secs_idx = 0;
//number of generated GPS time pulses
static U16 gps_time_pulse_num = 0;

int main(void)
{
	const Message_s * ubxmsg;
	U16 msgPollCfgPrt = 0;

	//WDOG disable
	WDTCTL = WDTPW | WDTHOLD;

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

    // Configure internal reference
    while(REFCTL0 & REFGENBUSY);              // If ref generator busy, WAIT
    REFCTL0 |= REFTCOFF|REFVSEL_1|REFON;      // Turn of temp sensor to save power, select internal ref = 2.0V
                                              // Internal Reference ON
    //not neccessary..
    //while(!(REFCTL0 & REFGENRDY));            // Wait for reference generator
                                              // to settle

	// Configure ADC12
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;      // Sampling time, S&H=16, ADC12 on
    //Divide SMCLK/32 to be able to use ADC12PWRMD
    //single channel, single conversion
    ADC12CTL1 = ADC12PDIV1 | ADC12SHP;
    ADC12CTL2 = ADC12RES_2 | ADC12PWRMD;   // 12-bit conversion results, power mode ADCLK = 1/4*maxADCLK (5.4MHz/4)
    ADC12CTL3 = ADC12BATMAP;				// Put 1/2xAVcc to ADC input channel A31
    ADC12MCTL0 |= ADC12VRSEL_1|ADC12INCH_31;// A31 ADC input select; Vref=VREF
    ADC12IER0 |= ADC12IE0;                 // Enable ADC conv complete interrupt
    //enable conversion
    ADC12CTL0 |= ADC12ENC;


	//configure TIMERs_A
	//GREEN LED
	TA0CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA0CCR0 = 50000;
	TA0CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet
	//RED LED
	TA1CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA1CCR0 = 50000;
	TA1CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet
	//new timer for DELAY, TODO: merge green, red and delay timer
	TA2CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA2CCR0 = 50000;
	TA2CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet

	//GPS ubx message timouter
	TA3CCTL0 = CCIE;                          // TACCR0 interrupt enabled
	TA3CCR0 = 50000;
	TA3CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet

	//dbg_inituart();
	gps_inituart();
	spi_init();
	ubx_init();

	//enable interruputs
	__bis_SR_register(GIE);
	if (pcif_has_power())
	{
		pcif_enif();
		//USB will read/write SPI
		spi_enrx();
	}else
	{
		//disable PC communication
		pcif_disif();
		//without USB no reads from the SPI memory
		spi_disrx();
	}
	gps_pulse_dis();

	dbg_txmsg("\nWelcome to taGPS program. Last reset: ");
	U16 tmp;
	U8 txch;
	tmp = SYSRSTIV;
	dbg_txmsg("0x");
	//highbyte to hex:
	txch = (tmp>>12) & 0xf;
	txch = util_num2hex(&txch);
	dbg_txchar(txch);

	txch = (tmp>>8) & 0xf;
	txch = util_num2hex(&txch);
	dbg_txchar(txch);

	//lowbyte to hex:
	txch = (tmp>>4) & 0xf;
	txch = util_num2hex(&txch);
	dbg_txchar(txch);

	txch = tmp & 0xf;
	txch = util_num2hex(&txch);
	dbg_txchar(txch);
	txch = '\n';
	dbg_txchar(txch);
	txch = '\r';
	dbg_txchar(txch);
	dbg_txmsg("\nEnter the operational mode and sleep...");

	while (1)
	{

		if (!spi_txempty() && !(UCB0STATW & UCBUSY))
		{
			//send a character to FLASH
			UCB0TXBUF = spi_txchpop();
		}

		if (!cmdToDo)
		{
			//if (!gGpsPowered || (gGpsPowered && gGpsInitialized))
			{
				//nothing to do, fall asleep
				__bis_SR_register(LPM3_bits | GIE);     // Enter LPM3, interrupts enabled
				__no_operation();                       // For debugger
			}
		}

		if (gUsbPowerChange && gUsbPowered)
		{
			gUsbPowerChange = false;
			//enable PC communication
			pcif_enif();
			//USB will read/write SPI
			spi_enrx();
		}
		if (gUsbPowerChange && !gUsbPowered)
		{
			gUsbPowerChange = false;
			//disable PC communication
			pcif_disif();
			//without USB no reads from the SPI memory
			spi_disrx();
		}

		//is GPS powered?
		if (gGpsPowerChange && gGpsPowered)
		{
			gGpsPowerChange = false;
			gps_uart_enable();
			gps_ie(); //enable interrupts
			if (!gGpsInitialized)
			{
				//init_configure_gps();
				//prepare port cfg msg
				ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
				//send it
				gps_cmdtx(ubxmsg->pMsgBuff);
			}
		}
		if (gGpsPowerChange && !gGpsPowered)
		{
			gGpsPowerChange = false;
			gps_id(); //disable interrupt
			gps_uart_disable();
			gGpsInitialized = false;
			//flash long red that GPS chip is off power
			led_flash_red_long();
			gps_time_pulse_secs_idx = 0; //set GPS pulse period to default

		}

		if (!gGpsPowered)
		{
			if (cmdToDo & BUTTON1)
			{
				cmdToDo &= ~BUTTON1;
			}
		}
		if (gGpsPowered)
		{
			if (cmdToDo & BUTTON1)
			{
				cmdToDo &= ~BUTTON1;
				//configure GPS:
				but_yellow_disable();
				gps_time_pulse_secs_idx++;
				if (gps_time_pulse_secs_idx > gps_pulse_cfg_option_num-1)
				{
					gps_time_pulse_secs_idx = 0;
				}
				gBlinkRed = gps_pulse_cfg_ar[gps_time_pulse_secs_idx];
			}
		}

		if (gGpsPowered && gGpsInitialized)
		{
			if (cmdToDo & GPS_PULSE)
			{
				cmdToDo &= ~GPS_PULSE;
				gps_time_pulse_num++;
				if (gps_time_pulse_num >= gps_pulse_cfg_ar[gps_time_pulse_secs_idx])
				{
					//measure battery voltage
					ADC12CTL0 ^= ADC12SC;
					gps_time_pulse_num = 0;
					//dbg_txmsg("\nPoll GPS status");
					//dbg_txmsg("P.");
					ubxmsg = ubx_get_msg(MessageIdPollPvt);
					gps_cmdtx(ubxmsg->pMsgBuff);
					if (gpower_save_mode_activated)
					{
						led_flash_green_short();
					}else
					{
						led_flash_red_short();
					}
				}
			}
		}

		if (gGpsPowered && !gGpsInitialized)
		{
			if (cmdToDo & GPS_PULSE)
			{
				cmdToDo &= ~GPS_PULSE;
				//configure power save mode
				ubxmsg = ubx_get_msg(MessageIdPollCfgGnss);
				gps_cmdtx(ubxmsg->pMsgBuff);
				led_flash_green_short();
			}
		}

		if (cmdToDo & NXTGPSCMD)
		{
			cmdToDo &= ~NXTGPSCMD;

			if ((ubxmsg > 0) && (ubxmsg->confirmed))
			{
				//dbg_txmsg("C.", 2);
				switch (ubxmsg->id)
				{
				case MessageIdPollCfgPrt:
					ubxmsg = ubx_get_msg(MessageIdSetCfgPrt);
					break;
				case MessageIdSetCfgPrt:
					//ubxmsg = ubx_get_msg(MessageIdPollCfgGnss); configure power save mode later on
#ifdef ARTIFICIAL_GPS
					//Artificial GPS pulse
					TB0CCTL0 = CCIE;                          // TACCR0 interrupt enabled
					TB0CCR0 = 50000;
					TB0CTL = TASSEL__SMCLK | MC__STOP;        // SMCLK, do not count yet
					TB0CTL = TASSEL__SMCLK | ID__8 | MC__UP;        // SMCLK, start timer
					TB0EX0 = TAIDEX_7;
#endif
					ubxmsg = 0;
					led_flash_green_long();
					gps_pulse_en();
					msgPollCfgPrt++;
					break;
				case MessageIdPollCfgGnss:
//					gContinue = true;
//					while(0){
//						__bis_SR_register(GIE);
//					};
					ubxmsg = ubx_get_msg(MessageIdSetCfgGnss);
					break;
				case MessageIdSetCfgGnss:
//					gContinue = true;
//					while(0){
//						__bis_SR_register(GIE);
//					};
					//dbg_txmsg("\nPoll UBX CFG PM2 message: ");
					ubxmsg = ubx_get_msg(MessageIdPollCfgPm2);
					break;
				case MessageIdPollCfgPm2:
//					gContinue = true;
//					while(gContinue){
//						__bis_SR_register(GIE);
//					};
					//dbg_txmsg("\nGet UBX CFG RXM message: ");
					ubxmsg = ubx_get_msg(MessageIdGetCfgRxm);
					//msgPollCfgPrt = 0;
					break;
				case MessageIdGetCfgRxm:
//					gContinue = true;
//					while(gContinue){
//						__bis_SR_register(GIE);
//					};
					//configure power save
					ubxmsg = ubx_get_msg(MessageIdSetCfgRxm);
					//msgPollCfgPrt++;
					break;
				case MessageIdSetCfgRxm:
//					gContinue = true;
//					while(gContinue){
//						__bis_SR_register(GIE);
//					};
					//turn of the message confirmation
					//ubxmsg = 0;
					//check again if the power mode is set:
					ubxmsg = ubx_get_msg(MessageIdGetCfgRxm);
					//if (msgPollCfgPrt>1) ..DEBUG..
					if (1)
					{
						led_flash_green_long();
						if (!gGpsInitialized && !gGpsRestarted)
						{
							//store page indicating the start of a session:
							spiif_storeubx(0);
						}
						gGpsRestarted = false;
						gps_pulse_en();
						gGpsInitialized = true;
						//TEST without GPS pulse...
						//gps_pulse_en();
						//enable GPS RX interrupts
						UCA1IE |= UCRXIE;
						////TEST without GPS pulse...
						ubxmsg = 0;
					}
					break;
				case MessageIdPollPvt:
//					gContinue = true;
//					while(gContinue){
//						__bis_SR_register(GIE);
//					};
					spiif_storeubx(ubxmsg);
					if (ubxmsg->pBody->navPvt.flags > 3)
					{
						gpower_save_mode_activated = true;
						ubxmsg = 0;
						//blick green that we received gps pulse
						led_flash_green_short();
					}else
					{
						gpower_save_mode_activated = false;
						//configure power save mode again:
						ubxmsg = ubx_get_msg(MessageIdPollCfgGnss);
						gps_pulse_dis();
						led_flash_red_short();
					}
					//displej_this(ubxmsg->pBody->navPvt.year);
					break;
				default:
					led_flash_red_long();
					break;
				}
			}
			//send again the old or the next command except MessageIdPollPvt
			if (ubxmsg->id == MessageIdPollPvt)
			{
				ubxmsg = 0;
			}
			if ((ubxmsg > 0))
			{
				//dbg_txmsg("P.", 2);
				//dbg_txmsg(".", 1);
				gps_cmdtx(ubxmsg->pMsgBuff);
			}
		}

		if (cmdToDo & GPSRXCHAR)
		{
			cmdToDo &= ~GPSRXCHAR;
			tmp = gps_rx_ubx_msg(ubxmsg, true);
			if (tmp == 5)
			{
				led_off_red();
				led_flash_green_short();
				__no_operation();
			}else
			{
				if(tmp > 0 && gGpsInitialized)
				{
					//probably GPS has restarted... turn of NMEA
					gGpsInitialized = false;
					gGpsRestarted = true;
					gps_pulse_dis();
					ubxmsg = ubx_get_msg(MessageIdPollCfgPrt);
					//send it
					gps_cmdtx(ubxmsg->pMsgBuff);
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

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG))
	{
	case ADC12IV_NONE:        break;        // Vector  0:  No interrupt
	case ADC12IV_ADC12OVIFG:  break;        // Vector  2:  ADC12MEMx Overflow
	case ADC12IV_ADC12TOVIFG: break;        // Vector  4:  Conversion time overflow
	case ADC12IV_ADC12HIIFG:  break;        // Vector  6:  ADC12BHI
	case ADC12IV_ADC12LOIFG:  break;        // Vector  8:  ADC12BLO
	case ADC12IV_ADC12INIFG:  break;        // Vector 10:  ADC12BIN
	case ADC12IV_ADC12IFG0:                 // Vector 12:  ADC12MEM0 Interrupt
		gAdcBatteryVal = ADC12MEM0;                 // read out the result register (only 8 bit conversion??
		break;                                // Clear CPUOFF bit from 0(SR)
	case ADC12IV_ADC12IFG1:   break;        // Vector 14:  ADC12MEM1
	case ADC12IV_ADC12IFG2:   break;        // Vector 16:  ADC12MEM2
	case ADC12IV_ADC12IFG3:   break;        // Vector 18:  ADC12MEM3
	case ADC12IV_ADC12IFG4:   break;        // Vector 20:  ADC12MEM4
	case ADC12IV_ADC12IFG5:   break;        // Vector 22:  ADC12MEM5
	case ADC12IV_ADC12IFG6:   break;        // Vector 24:  ADC12MEM6
	case ADC12IV_ADC12IFG7:   break;        // Vector 26:  ADC12MEM7
	case ADC12IV_ADC12IFG8:   break;        // Vector 28:  ADC12MEM8
	case ADC12IV_ADC12IFG9:   break;        // Vector 30:  ADC12MEM9
	case ADC12IV_ADC12IFG10:  break;        // Vector 32:  ADC12MEM10
	case ADC12IV_ADC12IFG11:  break;        // Vector 34:  ADC12MEM11
	case ADC12IV_ADC12IFG12:  break;        // Vector 36:  ADC12MEM12
	case ADC12IV_ADC12IFG13:  break;        // Vector 38:  ADC12MEM13
	case ADC12IV_ADC12IFG14:  break;        // Vector 40:  ADC12MEM14
	case ADC12IV_ADC12IFG15:  break;        // Vector 42:  ADC12MEM15
	case ADC12IV_ADC12IFG16:  break;        // Vector 44:  ADC12MEM16
	case ADC12IV_ADC12IFG17:  break;        // Vector 46:  ADC12MEM17
	case ADC12IV_ADC12IFG18:  break;        // Vector 48:  ADC12MEM18
	case ADC12IV_ADC12IFG19:  break;        // Vector 50:  ADC12MEM19
	case ADC12IV_ADC12IFG20:  break;        // Vector 52:  ADC12MEM20
	case ADC12IV_ADC12IFG21:  break;        // Vector 54:  ADC12MEM21
	case ADC12IV_ADC12IFG22:  break;        // Vector 56:  ADC12MEM22
	case ADC12IV_ADC12IFG23:  break;        // Vector 58:  ADC12MEM23
	case ADC12IV_ADC12IFG24:  break;        // Vector 60:  ADC12MEM24
	case ADC12IV_ADC12IFG25:  break;        // Vector 62:  ADC12MEM25
	case ADC12IV_ADC12IFG26:  break;        // Vector 64:  ADC12MEM26
	case ADC12IV_ADC12IFG27:  break;        // Vector 66:  ADC12MEM27
	case ADC12IV_ADC12IFG28:  break;        // Vector 68:  ADC12MEM28
	case ADC12IV_ADC12IFG29:  break;        // Vector 70:  ADC12MEM29
	case ADC12IV_ADC12IFG30:  break;        // Vector 72:  ADC12MEM30
	case ADC12IV_ADC12IFG31:  break;        // Vector 74:  ADC12MEM31
	case ADC12IV_ADC12RDYIFG: break;        // Vector 76:  ADC12RDY
	default: break;
	}
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
	//keep button disabled during signaling
	//and call enable button enable timer
	but_yellow_disable();
}


// Timer2_A0 for delays interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER2_A0_VECTOR
__interrupt void Timer2_A0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER2_A0_VECTOR))) Timer2_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	TA2CTL = MC__STOP;        // SMCLK, do not count yet
	TA2EX0 = TAIDEX_0; 		  //clear extended divider

	{
		if (gBlinkRed > 0)
		{
			gBlinkRed--;
			led_flash_red_short();
		}
		if (gBlinkRed == 0)
		{
			but_yellow_enable();
		}
	}
}

// Timer3_A0 for delays interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER3_A0_VECTOR
__interrupt void Timer3_A0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER3_A0_VECTOR))) Timer3_A0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	TA3CTL = MC__STOP;        // SMCLK, do not count yet
	TA3EX0 = TAIDEX_0; 		  //clear extended divide

	if (gps_get_txbusy())
	{
		gps_set_txbusy(false);
		//wake up and look if message is confirmed
		cmdToDo |= NXTGPSCMD;

		//disable new RX chars receiving
		//TEST: koment this when not using the GPS PULSE..
		//UCA1IE &= ~UCRXIE;

		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();

	}
}

// Timer3_B7 for delays interrupt service routine
#ifdef ARTIFICIAL_GPS
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = TIMER0_B0_VECTOR
__interrupt void Timer0_B0_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(TIMER)_B0_VECTOR))) Timer0_B0_ISR (void)
#else
#error Compiler not supported!
#endif
{
	__no_operation();
	gGpsDiv += 1;
	if (gGpsDiv > 8)
	{
		gGpsDiv = 0;
		P2IFG |= BIT1;  //generate GPS pulse interrupt
	}
}
#endif //ARTIFICIAL GPS

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
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		break;
	case USCI_UART_UCTXIFG:
		//clear complete transfer
		UCA0IFG &= ~UCTXCPTIFG;
		if (!buff_empty())
		{
			UCA0TXBUF = buff_pop();
		}else
		{
			//cover blind spot by setting
			UCA0IE |= UCTXCPTIE;
		}
		break;
	case USCI_UART_UCSTTIFG: break;
	case USCI_UART_UCTXCPTIFG:
		//turn off blind stop
		UCA0IE &= ~UCTXCPTIE;
		if (!buff_empty())
		{
			UCA0TXBUF = buff_pop();
		}else
		{
			//prepare for next buffer push()
			UCA0IFG |= UCTXCPTIFG;
		}
		break;
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
		P7OUT ^= BIT7;
		cmdToDo |= GPSRXCHAR;
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		break;
	case USCI_UART_UCTXIFG:
		if (!gps_txempty())
		{
			UCA1TXBUF = gps_txbpop();
		}else
		{
			//clear flag
			UCA1IFG &= ~UCTXCPTIFG;
			//wait until the last char is sent out
			UCA1IE |= UCTXCPTIE;
		}
		break;
	case USCI_UART_UCSTTIFG: break;
	case USCI_UART_UCTXCPTIFG:
		//if we configure GPS start timeout
		//if (!gGpsInitialized)
		{
			led_flash_msg_long();
		}
		//turn off this interrupt
		UCA1IE &= ~UCTXCPTIE;
		//clear flag
		//UCA1IFG |= UCTXCPTIFG;
		break;
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
	case P2IV_P2IFG0: break;
	case P2IV_P2IFG1:
		//GPS TIME PULSE:
		cmdToDo |= GPS_PULSE;
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		//23*4 bajtu je NAV message...
		//264 bajtu/stranka
		//mame 32768 stranek - 9hodin->stranka/zaznam/sekunda
		break;
	case P2IV_P2IFG2:
		if (gps_has_power())
		{
			gGpsPowered = true;
			//we have power ('1'), wait for 1->0 transition:
			P2IES |= BIT2;                                // P2.2 Hi-to-Lo edge
			gGpsPowerChange = true;

		}else
		{
			gGpsPowered = false;
			//we have NO power ('0'), wait for 0->1 transition:
			P2IES &= ~BIT2;                                // P2.2 Lo-to-Hi edge
			gGpsPowerChange = true;
		}
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		break;
	case P2IV_P2IFG3: break;
	case P2IV_P2IFG4: break;
	case P2IV_P2IFG5: break;
	case P2IV_P2IFG6: break;
	case P2IV_P2IFG7: break;
	}
}

// Port 3 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=PORT3_VECTOR
__interrupt void Port_3(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(PORT3_VECTOR))) Port_3 (void)
#else
#error Compiler not supported!
#endif
{
	switch(__even_in_range(P3IV, P3IV_P3IFG7))
	{
	case P3IV_NONE: break;
	case P3IV_P3IFG0: break;
	case P3IV_P3IFG1: break;
	case P3IV_P3IFG2:
		if (pcif_has_power())
		{
			gUsbPowered = true;
			//we have power ('1'), wait for 1->0 transition:
			P3IES |= BIT2;                                // P3.2 Hi-to-Lo edge
			gUsbPowerChange = true;

		}else
		{
			gUsbPowered = false;
			//we have NO power ('0'), wait for 0->1 transition:
			P3IES &= ~BIT2;                                // P3.2 Lo-to-Hi edge
			gUsbPowerChange = true;
		}
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		break;
	case P3IV_P3IFG3: break;
	case P3IV_P3IFG4: break;
	case P3IV_P3IFG5: break;
	case P3IV_P3IFG6: break;
	case P3IV_P3IFG7:
		//gContinue = false; clear following for better testing...
		cmdToDo |= BUTTON1;
		__bic_SR_register_on_exit(LPM3_bits | GIE);     // Exit LPM3
		__no_operation();
		break;
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
