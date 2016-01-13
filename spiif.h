/*
 * spiif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef SPIIF_H_
#define SPIIF_H_
#include <msp430.h>
#include "typedefs.h"
#include "ubxprot.h"

#define SPI_ADDR_SIZE 	4     //OPCODE, page address byte1,2, buffer address byte
#define MEM_PAGE_SIZE 	264
#define MEM_MAP_SIZE 	24

#define SPI_PG_READ_DUMMY_BYTES 	4
#define SPI_TX_SIZE (MEM_PAGE_SIZE+SPI_PG_READ_DUMMY_BYTES+SPI_ADDR_SIZE+4) //4 = margin
#define SPI_RX_SIZE (MEM_PAGE_SIZE+4) //4 = margin
//#define SPI_RX_SIZE (20+4) //4 = margin

extern const U8 * spiif_pmmap;

void spi_init(void);
void spi_disrx(void);
void spi_enrx(void);
void spiif_storeubx(const Message_s * ubx);
void spi_getstat(void);
U16 spi_getpgnum(void);
void spi_clrpgnum(void);

void spi_loadpg(void);

inline void spi_txchpush(U8 * ch);
inline void spi_rxchpush(void);


//functions used in interrupts, force to inline it:

extern U8* ptx_buff;

extern U16* pg_txput;
extern U16 * pg_txpop;

extern U16 * pg_rxspi_txpc_put;
extern U8 * pg_rxspi_txpc_buff;

inline U8 spi_txchpop(void)
{
	U8 ret;

	ret = ptx_buff[*pg_txpop];
	(*pg_txpop)++;
	if (*pg_txpop == SPI_TX_SIZE)
	{
		*pg_txpop = 0;
	}

	return ret;
}

#pragma FUNC_ALWAYS_INLINE(spi_txempty)
inline Boolean spi_txempty(void)
{
	Boolean ret = false;

	if (*pg_txput == *pg_txpop)
	{
		ret = true;
	}
	return ret;
}

#pragma FUNC_ALWAYS_INLINE(spi_rxchpush)
inline void spi_rxchpush(void)
{
	pg_rxspi_txpc_buff[*pg_rxspi_txpc_put] = UCB0RXBUF;
	(*pg_rxspi_txpc_put)++;
	if (*pg_rxspi_txpc_put == SPI_RX_SIZE)
	{
		*pg_rxspi_txpc_put = 0;
	}
}

#endif /* SPIIF_H_ */
