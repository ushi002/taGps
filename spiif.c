/*
 * spiif.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#include <string.h>
#include <msp430.h>
#include "spiif.h"
#include "dbgif.h"

extern U16 gAdcBatteryVal;

		//23*4 bajtu je NAV message...
		//264 bajtu/stranka/8xUBX
		//mame 32768 stranek*8 sekund = 72.8 hodin (3.03dny) ubx/sekunda

//MAXIMUM 33 BYTES!!!:
static const U8 ar_ubxnavpvt_cfg[] = {
//		0,1,2,3, 	//U32 iTOW;
		4,5, 		//U16 year;
	    6, 			//U8  month;
	    7, 			//U8  day;
	    8, 			//U8  hour;
	    9,			//U8  min;
	    10, 		//U8  sec;
	    11, 		//U8  valid;
//	    12, 13, 14, 15, 	//U32 tAcc;
//	    16, 17, 18, 19, 	//I32 nano;
	    20,					//U8  fixType;
	    21, 		//U8  flags;
//	    22, 		//U8  reserved1;
	    23, 		//U8  numSV;
	    24, 25, 26, 27, 	//I32 lon;
	    28, 29, 30, 31,		//I32 lat;
	    32, 33, 34, 35, 	//I32 height;
//	    36, 37, 38, 39, 	//I32 hMSL;
//	    40, 41, 42, 43, 	//U32 hAcc;
//	    44, 45, 46, 47, 	//U32 vAcc;
//	    48, 49, 50, 51, 	//I32 velN;
//	    52, 53, 54, 55, 	//I32 velE;
//	    56, 57, 58, 59, 	//I32 velD;
	    60, 61, 62, 63, 	//I32 gSpeed;
//	    64, 65, 66, 67, 	//I32 headMot;
//	    68, 69, 70, 71, 	//U32 sAcc;
//	    72, 73, 74, 75, 	//U32 headAcc;
//	    76, 77, 		//U16 pDOP;
//	    78, 79, 		//U16 reserved2a;
//	    80, 81, 82, 83, 	//U32 reserved2b;
//	    84, 85, 86, 87, 	//I32 HeadVeh;
		//TOOD: put here battery, temperature HouseKeeping?:
	    88, 89, 90, 91 		//U32 reseverd3;
};

static U8 tx_buff[SPI_TX_SIZE];
static U8 g_rxspi_txpc_buff[SPI_RX_SIZE];

static U8 memory_map[MEM_MAP_SIZE] = "Ahoj\r\n";
const U8 * spiif_pmmap;

static U16 g_txput = 0;
static U16 g_txpop = 0;
U16 * pg_txput;
U16 * pg_txpop;

static U16 g_rxpop = 0;
static U16 g_rxput = 0;
U16 * pg_rxspi_txpc_pop;
U16 * pg_rxspi_txpc_put;
U8 * pg_rxspi_txpc_buff;

U8 * ptx_buff;

#pragma PERSISTENT(g_pages_stored)
static U16 g_pages_stored = 0;

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
	UCB0BR0 = 64;
	UCB0BR1 = 0x00;

	UCB0CTLW0 &= ~UCSWRST;                    // **Initialize USCI state machine**
	UCB0IE |= UCRXIE | UCTXIE;                         // Enable USCI_B0 RX, TX interrupt

	spiif_pmmap = memory_map;

	ptx_buff = tx_buff;
	pg_txput = &g_txput;
	pg_txpop = &g_txpop;

	pg_rxspi_txpc_buff = g_rxspi_txpc_buff;
	pg_rxspi_txpc_put = &g_rxput;
	pg_rxspi_txpc_pop = &g_rxpop;
}

void spi_disrx(void)
{
	UCB0IE &= ~UCRXIE;
}
void spi_enrx(void)
{
	UCB0IFG &= ~UCRXIFG;
	UCB0IE |= UCRXIE;
}

static void spi_pgstore(void)
{
	if (g_pages_stored+1 == MEM_MAX_PAGES)
	{
		dbg_txmsg("[!!] SPI memory is full! Won't store any UBX messages!");
		return;
	}

	tx_buff[0] = 0x82; //opcode: write mem page through buffer1
	//for 264 bytes:
	tx_buff[1] = (U8) (g_pages_stored >> 7); //page addres byte 1
	tx_buff[2] = (U8) ((g_pages_stored << 1) & 0x01fe); //page addres byte 2
	tx_buff[3] = 0x00; //buffer byte addres offset

	g_txput = SPI_ADDR_SIZE+MEM_PAGE_SIZE;
	g_txpop = 0;
	g_pages_stored++;

	UCB0TXBUF = spi_txchpop();
}

U16 spi_getpgnum(void)
{
	return g_pages_stored;
}

void spi_clrpgnum(void)
{
	g_pages_stored = 0;
}

void spi_getstat(void)
{
	U8 byte;

	byte = 0xD7; //status opcode
	spi_txchpush(&byte);
	byte = 0x00; //2x dummy
	spi_txchpush(&byte);
	spi_txchpush(&byte);
}

void spi_loadpg(U16 pgNum)
{
	tx_buff[0] = 0xd2; //opcode: write mem page through buffer1
	//for 264 bytes:
	tx_buff[1] = (U8) (pgNum >> 7); //page addres byte 1
	tx_buff[2] = (U8) ((pgNum << 1) & 0x01fe); //page addres byte 2
	tx_buff[3] = 0x00; //buffer byte addres offset
	//4x dummybytes
	//264 bytes
	g_txput = SPI_ADDR_SIZE+SPI_PG_READ_DUMMY_BYTES+MEM_PAGE_SIZE;
	g_txpop = 0;
}

void spiif_storeubx(const Message_s * ubx)
{
	U16 i;
	static U16 msgNum = 0;
	U16 actualMemOffset;
	const U16 MESSGS_PER_PAGE = 8; //8 messages per page
	const U16 NAV_MEM_OFFSET = MEM_PAGE_SIZE/MESSGS_PER_PAGE;

	if (!spi_txempty())
	{
		dbg_txmsg("\nCannot store UBX message! SPI TX buffer is not empty!");
		return;
	}

	U8 * msg_pointer;

	if (ubx->id == MessageIdPollPvt)
	{
		if (msgNum < MESSGS_PER_PAGE)
		{
			msg_pointer = (U8 *) &(ubx->pBody->navPvt);
			actualMemOffset = msgNum*NAV_MEM_OFFSET;
			for (i=0; i<sizeof(ar_ubxnavpvt_cfg)-4; i++)
			{
				tx_buff[SPI_ADDR_SIZE+i+actualMemOffset] = msg_pointer[ar_ubxnavpvt_cfg[i]];
			}
			//little endianness:
			tx_buff[SPI_ADDR_SIZE+actualMemOffset+sizeof(ar_ubxnavpvt_cfg)-4] = (U8) (gAdcBatteryVal);
			tx_buff[SPI_ADDR_SIZE+actualMemOffset+sizeof(ar_ubxnavpvt_cfg)-3] = (U8) (gAdcBatteryVal>>8);
			tx_buff[SPI_ADDR_SIZE+actualMemOffset+sizeof(ar_ubxnavpvt_cfg)-2] = (U8) (gAdcBatteryVal);
			tx_buff[SPI_ADDR_SIZE+actualMemOffset+sizeof(ar_ubxnavpvt_cfg)-1] = (U8) (gAdcBatteryVal>>8);

			msgNum++;
		}
		if (msgNum >= MESSGS_PER_PAGE)
		{
			dbg_txmsg("\n\tSTORE UBX message");
			spi_pgstore();
			msgNum = 0;
		}
	}else
	{
		dbg_txmsg("\nWRONG ubx message!");
	}
}

inline void spi_txchpush(U8 * ch)
{
	tx_buff[g_txput] = *ch;
	g_txput++;
	if (g_txput == SPI_TX_SIZE)
	{
		g_txput = 0;
	}
}

