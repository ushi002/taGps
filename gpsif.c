/*
 * gpsif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <msp430.h>
#include "typedefs.h"

void gps_initport(void)
{
	// Configure GPIO
	P3SEL0 |= BIT4 | BIT5;                    // USCI_A1 UART operation on P3
	P3SEL1 &= ~(BIT4 | BIT5);
}

void gps_inituart(void)
{
	// Configure USCI_A1 for UART mode
	UCA1CTLW0 = UCSWRST;                      // Put eUSCI in reset
	UCA1CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	// Baud Rate calculation
	// 8000000/(16*9600) = 52.083
	// Fractional portion = 0.083
	// User's Guide Table 21-4: UCBRSx = 0x04
	// UCBRFx = int ( (52.083-52)*16) = 1
	UCA1BR0 = 52;                             // 8000000/16/9600
	UCA1BR1 = 0x00;
	UCA1MCTLW |= UCOS16 | UCBRF_1 | 0x4900;
	UCA1CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
	UCA1IE |= UCRXIE;                         // Enable USCI_A1 RX interrupt
}

void gps_cmdtx(U8 * buff, U16 len)
{
	U16 i;

	//add sync bytes
	buff[0] = 0xb5;
	buff[1] = 0x62;

	for(i = 0; i < len; i++)
	{
		while(!(UCA1IFG & UCTXIFG));			//wait until UART0 TX buffer is empty
		while(!(UCA0IFG & UCTXIFG));			//wait until UART0 TX buffer is empty
		UCA1TXBUF = buff[i];
		UCA0TXBUF = i + 48; //get decimal number
	}
	//P6OUT ^= BIT5 | BIT6;                   // Toggle LEDs
}
