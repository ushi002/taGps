/*
 * gpsif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <msp430.h>
#include "gpsif.h"
#include "dbgif.h"

static U8 gpsumsg[82]; //GPS UBX message
static UbxPckHeader_s * pGpsUMsgHead = (UbxPckHeader_s *) gpsumsg;

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
}

//Interrupt enable
void gps_uart_ie(void)
{
	UCA1IE |= UCRXIE;                         // Enable USCI_A1 RX interrupt
}

//Interrupt disable
void gps_uart_id(void)
{
	UCA1IE &= ~UCRXIE;                         // Disable USCI_A1 RX interrupt
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
		while(!(UCA1IFG & UCTXIFG));			//wait until UART0 TX buffer is empty
		UCA1TXBUF = buff[i];
	}
	//P6OUT ^= BIT5 | BIT6;                   // Toggle LEDs
}

/** @brief Check if a new char is in buffer
 *
 * @return  - false - no new character
 * 			- true - new character loaded to rxChar */
Boolean gps_newchar(U8 * rxChar)
{
	Boolean retVal = false;

	if (UCA1IFG & UCRXIFG)
	{
		*rxChar = UCA1RXBUF;
		retVal = true;
	}

	return retVal;
}


/** @brief Process received character from GPS
 *
 * @return  - 0 done *
 * 			- 1 not a UBX message
 *          - 2 need one more loop to process the packet
 *          - 3 done w/ error: message too long
 *          - 4 done w/ error: wrong message checksum
 *          - 5 done, message received successfully */
U16 gps_rxchar(const Message_s * lastMsg)
{
	static ubxstat_e ubxstat = ubxstat_idle;
	static U16 ubxBytePos = 0;
	static U16 charMissed = 0;

	U8 rxChar;
	U16 retVal = 0;

	rxChar = UCA1RXBUF;

	switch (ubxstat)
	{
	case ubxstat_idle:
		if (rxChar == 0xB5)
		{
			ubxstat = ubxstat_syncchar2;
			ubxBytePos = 0;
			gpsumsg[ubxBytePos++] = rxChar;
			charMissed = 0;
		}
		else
		{
			charMissed++;
		}
		if (charMissed > 200)
		{
			//can not find UBX message among receiving characters, quit
			retVal = 1;
		}
		break;
	case ubxstat_syncchar2:
		gpsumsg[ubxBytePos++] = rxChar;
		ubxstat = ubxstat_msgclass;
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
			ubxstat = ubxstat_process;
			retVal = 2;

		}
		if (pGpsUMsgHead->length > MAX_MESSAGEBUF_LEN)
		{
			retVal = 3;
			ubxstat = ubxstat_idle;
			__no_operation();
		}
		break;
	case ubxstat_process:
		//process packet
		if (!ubx_checkmsg(gpsumsg))
		{
			//message is OK
			ubx_msgst(lastMsg, gpsumsg);
			retVal = 5;
		}
		else
		{
			//message error
			dbg_txerrmsg(1);
			retVal = 4;
		}
		ubxstat = ubxstat_idle;
		break;
	default: //error state
		P6OUT ^= BIT5;	//toggle led
		__no_operation();
		break;
	}

	return retVal;
}
