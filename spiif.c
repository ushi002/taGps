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

		//23*4 bajtu je NAV message...
		//264 bajtu/stranka
		//mame 32768 stranek - 9hodin->stranka/zaznam/sekunda

static U8 tx_buff[SPI_TX_SIZE];
static U8 rx_buff[SPI_RX_SIZE];

static U8 mem_page[MEM_PAGE_SIZE];

static U8 memory_map[MEM_MAP_SIZE] = "Ahoj\r\n";
const U8 * spiif_pmmap;

static U16 g_txput = 0;
static U16 g_txpop = 0;
U16 * pg_txput;
U16 * pg_txpop;

static U16 g_rxpop = 0;
static U16 g_rxput = 0;
U16 * pg_rxpop;
U16 * pg_rxput;

U8 * ptx_buff;
U8 * prx_buff;

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
	UCB0IE |= UCRXIE | UCTXIE;                         // Enable USCI_B0 RX, TX interrupt

	spiif_pmmap = memory_map;

	ptx_buff = tx_buff;
	prx_buff = rx_buff;
	pg_txput = &g_txput;
	pg_txpop = &g_txpop;

	pg_rxput = &g_rxput;
	pg_rxpop = &g_rxpop;
}

void spi_pgstore(void)
{
//	cmd[0] = 0x82; //write mem page through buffer1
//	cmd[1] = 0x00;
//	cmd[2] = 0x00; //1,2 page number to write
//	cmd[3] = 0x00; //buffer address offset to start write with
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

void spiif_storeubx(const Message_s * ubx)
{
	U16 i;
	static Boolean second_half = false;
	const U16 HALF_MEM_POSITION = MEM_PAGE_SIZE>>1;

	U8 * msg_pointer;

	if (ubx->id == MessageIdPollPvt)
	{
		if (!second_half)
		{
			msg_pointer = (U8 *) &(ubx->pBody->navPvt);
			for (i=0; i<sizeof(UbxNavPvt_s); i++)
			{
				mem_page[i] = msg_pointer[i];
			}
			second_half = true;
		}else
		{
			//fill second half of mem page
			msg_pointer = (U8 *) &(ubx->pBody->navPvt);
			for (i=0; i<sizeof(UbxNavPvt_s); i++)
			{
				mem_page[i+HALF_MEM_POSITION] = msg_pointer[i];
			}
			second_half = false;
			spi_pgstore();
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

inline U8 spi_rxchpop(void)
{
	U8 ret;

	ret = rx_buff[g_rxpop];
	g_rxpop++;
	if (g_rxpop == SPI_RX_SIZE)
	{
		g_rxpop = 0;
	}

	return ret;
}
