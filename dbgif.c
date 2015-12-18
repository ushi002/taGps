/*
 * dbgif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <string.h>
#include <msp430.h>
#include "dbgif.h"

static U8 err_msg[6][ERR_MSG_LEN] = {
		"1234567890123456789012345678901234567890",
		"[!!] RX MSG checksum error!           \n",
		"[!!] Panic in ubx_msgst()!            \n",
		"[!!] No message to load config from!  \n",
		"[!!] Received ACK from unknown MSG!   \n",
		"[!!] Expected ACK, but not received!  \n"
};

void dbg_initport(void)
{
	// Configure GPIO
	P4SEL0 |= BIT2 | BIT3;                    // USCI_A0 UART operation on P4
	P4SEL1 &= ~(BIT2 | BIT3);
}

void dbg_inituart(void)
{
	// Configure USCI_A0 for UART mode
	UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
	UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
	// Baud Rate calculation
	// 8000000/(16*9600) = 52.083
	// Fractional portion = 0.083
	// User's Guide Table 21-4: UCBRSx = 0x04
	// UCBRFx = int ( (52.083-52)*16) = 1
	UCA0BR0 = 52;                             // 8000000/16/9600   for  9600baudrate
	UCA0BR0 = 26;                             // 8000000/16/9600/2 for 19200baudrate
	UCA0BR1 = 0x00;
	UCA0MCTLW |= UCOS16 | UCBRF_1 | 0x4900;
	UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
	//UCA0IE |= UCRXIE;                         // Enable USCI_A0 RX interrupt
}

void gps_txchars(U8 * buff, U16 len)
{

}

void dbg_txchar(U8 * pChar)
{
	while(!(UCA0IFG&UCTXIFG)); //wait until UCA0 TX empty
	UCA0TXBUF = *pChar;
}

void dbg_txerrmsg(U8 err_code)
{
	I16 i;

	for (i = 0; i < ERR_MSG_LEN; i++)
	{
		while(!(UCA0IFG&UCTXIFG)); //wait until UCA0 TX empty
		UCA0TXBUF = err_msg[err_code][i];
	}
}

void dbg_txmsg(char * msg)
{
	I16 i;
	int len;

	len = strlen(msg);

	for (i = 0; i < len; i++)
	{
		while(!(UCA0IFG&UCTXIFG)); //wait until UCA0 TX empty
		if (msg[i] == '\n')
		{
			//when a new line requested
			//insert carriage return
			UCA0TXBUF = '\r';
			while(!(UCA0IFG&UCTXIFG)); //wait until UCA0 TX empty
		}
		UCA0TXBUF = msg[i];
	}
}
