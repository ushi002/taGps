/*
 * dbgif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <string.h>
#include <msp430.h>
#include "dbgif.h"
#include "util.h"

extern U8 * pg_rxspi_txpc_buff;
extern U16 * pg_rxspi_txpc_pop;
extern U16 * pg_rxspi_txpc_put;

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
	UCA0BR0 = 52;                             // 8000000/16/9600   for  9600baudrate, UCBRF_1, UCBRSx should be 0x49
	UCA0BR0 = 26;                             // 8000000/16/9600/2 for 19200baudrate, UCBRF_0, UCBRSx should be 0xB6
	UCA0BR1 = 0x00;
	UCA0MCTLW |= UCOS16 | UCBRF_0 | 0xB600;
	UCA0CTLW0 &= ~UCSWRST;                    // Release eUSCI reset

	//prepare complete transfer for first transfer
	UCA0IFG |= UCTXCPTIFG;
	//clear pending TX buffer empty flag which is set when UCSWRST=1
	UCA0IFG &= ~UCTXIFG;
	UCA0IE |= UCTXIE;

}

void gps_txchars(U8 * buff, U16 len)
{

}

void dbg_txerrmsg(U8 err_code)
{
//	I16 i;
//
//	for (i = 0; i < ERR_MSG_LEN; i++)
//	{
//		dbg_txchar(&err_msg[err_code][i]);
//	}
}

void dbg_txmsg(char * msg)
{
	I16 i;
	int len;

	len = strlen(msg);

	//while(!buff_empty());

	for (i = 0; i < len; i++)
	{
		buff_put(msg[i]);
		if (msg[i] == '\n')
		{
			buff_put('\r');
		}
	}
	if(UCA0IFG & UCTXCPTIFG)
	{
		//we have empty UART TX character buffer
		//and the previous TX is complete
		//enable the transmission now
		UCA0IE |= UCTXCPTIE;
	}
}

void dbg_txchar(U8 c)
{
	buff_put(c);

	if(UCA0IFG & UCTXCPTIFG)
	{
		//we have empty UART TX character buffer
		//and the previous TX is complete
		//enable the transmission now
		UCA0IE |= UCTXCPTIE;
	}
}

void pcif_rxchar(void)
{
	char rxch;
	U8 txch;
	U16 tmp, i, pgtot;

	rxch = UCA0RXBUF;

	switch (rxch)
	{
	case 's':
		//export status
//#define OUTPUT_PRINT_HEX
#ifdef OUTPUT_PRINT_HEX

		tmp = spi_getpgnum();
		dbg_txmsg("\n0x");
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

#else //OUTPUT_PRINT_HEX

		tmp = spi_getpgnum();
		//highbyte to hex:
		txch = (tmp>>8) & 0xff;
		dbg_txchar(txch);
		txch = (tmp) & 0xff;
		dbg_txchar(txch);

#endif //OUTPUT_PRINT_HEX

		//load SPI stat
		spi_getstat();
		break;
	case 'r':
		//reset memory page counter
		spi_clrpgnum();
		break;
	case 'd':
		//dump
		pgtot = spi_getpgnum();
		for(i = 0; i < pgtot; i++)
		{
			spi_loadpg(i);

			while(UCB0STATW & UCBUSY); //wait until SPI is not busy
			while(UCA0STATW & UCBUSY); //wait until TX PC uart is not busy

			UCB0TXBUF = spi_txchpop();
			while(!spi_txempty())
			{
				//we need GIE!
				__bis_SR_register(GIE);
			}

			//we have now the page loaded, send it to uart...
			//*pg_rxspi_txpc_put = MEM_PAGE_SIZE;
			//*pg_rxspi_txpc_pop = 0;
			UCA0TXBUF = buff_pop();
			while(!buff_empty());
		}
		break;
	default:
		dbg_txmsg(&rxch);
		break;
	}
	//transmit done interrupt flag to start new transmission:
	//UCA0IFG |= UCTXIFG;
}

/** Enable receiving PC UART commands telemetry */
void pcif_enif(void)
{
	dbg_inituart();
//	//set the clock again:
//	UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
//	UCA0CTLW0 &= ~UCSWRST;                    // Release eUSCI reset
//	UCA0IFG &= ~UCRXIFG;
//
//	//prepare complete transfer for first transfer
//	UCA0IFG |= UCTXCPTIFG;
//	//clear pending TX buffer empty flag which is set when UCSWRST=1
//	UCA0IFG &= ~UCTXIFG;
	UCA0IE |= UCTXCPTIE;
//
	UCA0IE |= UCRXIE;
}

/** Disable receiving PC UART commands and telemetry */
void pcif_disif(void)
{
	UCA0IE &= ~(UCRXIE | UCTXIE);
	UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
}

/** @brief Check if USB powered
 *
 * @return  - false - no power
 * 			- true - USB powered */
Boolean pcif_has_power(void)
{
	Boolean ret_val = false;

	if (P3IN & BIT2)
	{
		ret_val = true;
	}

	return ret_val;
}
