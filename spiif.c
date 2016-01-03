/*
 * spiif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <string.h>
#include <msp430.h>
#include "spiif.h"

		//23*4 bajtu je NAV message...
		//264 bajtu/stranka
		//mame 32768 stranek - 9hodin->stranka/zaznam/sekunda

static U8 mem_page[MEM_PAGE_SIZE];

static U8 memory_map[MEM_MAP_SIZE] = "Ahoj\r\n";
const U8 * spiif_pmmap;

static void initport(void);

static void initport(void)
{
	// Configure GPIO
	P1SEL0 |= BIT5 | BIT4;                    // Configure UCB0 slave select and clk
	P1SEL0 |= BIT6 | BIT7;                    // Configure SOMI and MISO
}

void spi_init(void)
{
	//initialize port:
	initport();

	// Configure USCI_B0 for SPI operation
	UCB0CTLW0 = UCSWRST;                      // **Put state machine in reset**
											// 4-pin, 8-bit SPI master
	UCB0CTLW0 |= UCMST | UCSYNC | UCCKPH | UCMSB | UCMODE_2 | UCSTEM;
	//UCMST  : master mode
	//UCSYNC : SPI mode
	//UCMSB  : MSB first
	//UCCKPH : first edge data capture, second edge data change
	//UCMODE_2 : 4 wire SPI
	//UCSTEM : STE signal selects the slave (else detects master conflicts)
											// Clock polarity high, MSB
	/////////////////////////////////////
	UCB0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	//UCB0CTLW0 |= UCSSEL__ACLK;                // ACLK
	// SMCLK = 8Mz, divider is 4, SPI speed => 2MHz:
	UCB0BR0 = 16;
	UCB0BR1 = 0x00;

	UCB0CTLW0 &= ~UCSWRST;                    // **Initialize USCI state machine**
	//UCB0IE |= UCRXIE;                         // Enable USCI_B0 RX interrupt

	spiif_pmmap = memory_map;
}

//INTERRUPT ROUTINE:
//#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
//#pragma vector=USCI_B0_VECTOR
//__interrupt void USCI_B0_ISR(void)
//#elif defined(__GNUC__)
//void __attribute__ ((interrupt(USCI_B0_VECTOR))) USCI_B0_ISR (void)
//#else
//#error Compiler not supported!
//#endif
//{
//  switch(__even_in_range(UCB0IV, USCI_SPI_UCTXIFG))
//  {
//    case USCI_NONE: break;
//    case USCI_SPI_UCRXIFG:
//      UCB0IFG &= ~UCRXIFG;
//      break;
//    case USCI_SPI_UCTXIFG:
//      UCB0TXBUF = TXData;                   	// Transmit characters
//      UCB0IE &= ~UCTXIE;						// disable interrupt
//      break;
//    default: break;
//  }
//}

//READ FLASH STATUS bytes
//while (!(UCB0IFG & UCTXIFG)){};
//UCB0TXBUF = 0xD7;                   // Transmit characters
//while (!(UCB0IFG & UCTXIFG)){};
//UCB0TXBUF = 0x00;
//while (!(UCB0IFG & UCTXIFG)){};
//UCB0TXBUF = 0x00;
////dbg_txmsg("\n4\n");
////clear RXflag
//UCB0IFG &= ~UCRXIFG;
//while (!(UCB0IFG & UCRXIFG)){};
////dbg_txmsg("\n5\n");
//RXData = UCB0RXBUF;
