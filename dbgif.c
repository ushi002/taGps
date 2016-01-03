/*
 * dbgif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <string.h>
#include <msp430.h>
#include "dbgif.h"
#include "spiif.h"

static U8 msg_buff[MSG_SIZE];

const U8 * gp_dbgif_buff;
U16 g_txbput = 0;
U16 g_txbpop = 0;

U8 * gp_txbuff;
U16 * pg_txbput;
U16 * pg_txbpop;


//when received uknown character then it is echoed
U8 g_dbgif_echo = '0';

static U8 err_msg[6][ERR_MSG_LEN] = {
		"1234567890123456789012345678901234567890",
		"[!!] RX MSG checksum error!           \n",
		"[!!] Panic in ubx_msgst()!            \n",
		"[!!] No message to load config from!  \n",
		"[!!] Received ACK from unknown MSG!   \n",
		"[!!] Expected ACK, but not received!  \n"
};

inline static void buffi_inc(void)
{
	g_txbput++;
	if (g_txbput == MSG_SIZE)
	{
		g_txbput = 0;
	}
}

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

	UCA0IE |= UCTXIE;

	gp_dbgif_buff = msg_buff;

	gp_txbuff = msg_buff;
	pg_txbput = &g_txbput;
	pg_txbpop = &g_txbpop;
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

	//while(!buff_empty());

	for (i = 0; i < len; i++)
	{
		msg_buff[g_txbput] = msg[i];
		buffi_inc();
		if (msg[i] == '\n')
		{
			//when a new line requested
			//insert carriage return
			msg_buff[g_txbput] = '\r';
			buffi_inc();

		}
	}

	//start transmission:
	//UCA0IFG |= UCTXIFG;
}

void pcif_rxchar(void)
{
	char rxch;

	rxch = UCA0RXBUF;

	switch (rxch)
	{
	default:
		dbg_txmsg(&rxch);
		break;
	}
	//transmit done interrupt flag to start new transmission:
	UCA0IFG |= UCTXIFG;
}

/** Enable receiving PC UART commands telemetry */
void pcif_enif(void)
{
	UCA0IE |= UCRXIE;
}

/** Disable receiving PC UART commands and telemetry */
void pcif_disif(void)
{
	UCA0IE &= ~(UCRXIE);
}
