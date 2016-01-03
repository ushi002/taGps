/*
 * gpsif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <msp430.h>
#include "gpsif.h"
#include "dbgif.h"

static U8 txbuff[TXB_SIZE];

static U8 g_txput = 0;
static U8 g_txpop = 0;

U8 * pgps_txbuf;
U8 * pgps_txput;
U8 * pgps_txpop;

static U8 gpsumsg[82]; //GPS UBX message
static UbxPckHeader_s * pGpsUMsgHead = (UbxPckHeader_s *) gpsumsg;

void gps_initport(void)
{
	// Configure GPIO
	P3SEL0 |= BIT4 | BIT5;                    // USCI_A1 UART operation on P3
	P3SEL1 &= ~(BIT4 | BIT5);

	P2DIR &= BIT3;                           // Set P2.3 GPS PWR SENSE as input direction
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
	UCA1IFG &= ~UCRXIFG; //clear previous received character
	UCA1CTLW0 &= ~UCSWRST;                    // Initialize eUSCI


	pgps_txbuf = txbuff;
	pgps_txput = &g_txput;
	pgps_txpop = &g_txpop;
}

//Interrupt enable
void gps_uart_ie(void)
{
	P2IFG &= ~BIT1;
	P2IE |= BIT1;                             // P2.1 interrupt enable - GPS TIME-PULSE
	UCA1IE |= UCRXIE;                         // Enable USCI_A1 RX interrupt
	UCA1IE |= UCTXIE;
}

//Interrupt disable
void gps_uart_id(void)
{
	UCA1IE &= ~UCRXIE;                         // Disable USCI_A1 RX interrupt
	UCA1IE &= ~UCTXIE;
}

static void txb_push(U8 * ch)
{
	txbuff[g_txput++] = *ch;
	if (g_txput == TXB_SIZE)
	{
		g_txput = 0;
	}

}
void gps_cmdtx(U8 * buff)
{
	U16 i;
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) buff;

	//add sync bytes
	buff[0] = 0xb5;
	buff[1] = 0x62;

	for(i = 0; i < pHead->length+8; i++)
	{
		txb_push(&buff[i]);
	}
}

void gps_initcmdtx(U8 * buff)
{
	U16 i;
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) buff;

	//add sync bytes
	buff[0] = 0xb5;
	buff[1] = 0x62;

	for(i = 0; i < pHead->length+8; i++)
	{
		while(!(UCA1IFG & UCTXIFG));                    //wait until UART0 TX buffer is empty
		UCA1TXBUF = buff[i];
	}
}

/** @brief Check if the GPS chip has power
 *
 * @return  - false - GPS chip has no power
 * 			- true - GPS chip has power */
Boolean gps_has_power(void)
{
	Boolean ret_val = false;

	if (P2IN & BIT3)
	{
		ret_val = true;
	}

	return ret_val;
}

/** @brief Check if a new char is in buffer
 *
 * @return  - false - no new character
 * 			- true - new character loaded to rxChar */
Boolean gps_newchar(U8 * rxChar, Boolean interruptCall)
{
	Boolean retVal = false;

	if (interruptCall)
	{
		*rxChar = UCA1RXBUF;
		retVal = true;

	}else
	{
		if (UCA1IFG & UCRXIFG)
		{
			*rxChar = UCA1RXBUF;
			retVal = true;
		}
	}

	return retVal;
}


/** @brief Process received character from GPS
 *
 * @return  - 0 done for now
 *          - 3 done w/ error: message too long
 *          - 4 done w/ error: wrong message checksum
 *          - 5 done, message received successfully */
U16 gps_rx_ubx_msg(const Message_s * lastMsg, Boolean interruptCall)
{
	static ubxstat_e ubxstat = ubxstat_idle;
	static U16 ubxBytePos = 0;
	U16 retVal = 0;
	U8 rxChar;

	if (!gps_newchar(&rxChar, interruptCall))
	{
		return retVal;
	}

	switch (ubxstat)
	{
	case ubxstat_idle:
		if (rxChar == 0xB5)
		{
			ubxstat = ubxstat_syncchar2;
			ubxBytePos = 0;
			gpsumsg[ubxBytePos++] = rxChar;
		}
		break;
	case ubxstat_syncchar2:
		if (rxChar == 0x62)
		{
			gpsumsg[ubxBytePos++] = rxChar;
			ubxstat = ubxstat_msgclass;
		}else
		{
			ubxstat = ubxstat_idle;
		}
		break;
	case ubxstat_msgclass:
			gpsumsg[ubxBytePos++] = rxChar;
			ubxstat = ubxstat_msgid;
			break;
	case ubxstat_msgid:
			gpsumsg[ubxBytePos++] = rxChar;
			ubxstat = ubxstat_getlen;
			break;
	case ubxstat_getlen:
		gpsumsg[ubxBytePos++] = rxChar;
		if (ubxBytePos == 6)
		{
			ubxstat = ubxstat_rec;
		}
		break;
	case ubxstat_rec:
		gpsumsg[ubxBytePos++] = rxChar;
		if (ubxBytePos >= (pGpsUMsgHead->length + sizeof(UbxPckHeader_s) + sizeof(UbxPckChecksum_s)))
		{
			//message loaded, process packet
			if (!ubx_checkmsg(gpsumsg))
			{
				//message is OK
				ubx_msgst(lastMsg, gpsumsg);
				retVal = 5;
				ubxstat = ubxstat_idle;
			}
			else
			{
				//message CRC error
				retVal = 4;
				ubxstat = ubxstat_idle;
			}

		}
		if (pGpsUMsgHead->length > MAX_MESSAGEBUF_LEN)
		{
			retVal = 3;
			ubxstat = ubxstat_idle;
		}
		break;
	default: //error state
		__no_operation();
		break;
	}

	return retVal;
}
