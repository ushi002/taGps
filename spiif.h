/*
 * spiif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef SPIIF_H_
#define SPIIF_H_
#include "typedefs.h"
#include "ubxprot.h"

#define MEM_PAGE_SIZE 	264
#define MEM_MAP_SIZE 	24

#define SPI_TX_SIZE 16
#define SPI_RX_SIZE 16

extern const U8 * spiif_pmmap;

void spi_init(void);
void spiif_storeubx(const Message_s * ubx);
void spi_getstat(void);

inline void spi_txchpush(U8 * ch);
inline U8 spi_rxchpop(void);
inline void spi_rxchpush(void);



//functions used in interrupts, force to inline it:

extern U8* ptx_buff;
extern U8* prx_buff;

extern U16* pg_txput;
extern U16 * pg_txpop;

extern U16 * pg_rxpop;
extern U16 * pg_rxput;

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
	prx_buff[*pg_rxput] = UCB0RXBUF;
	(*pg_rxput)++;
	if (*pg_rxput == SPI_RX_SIZE)
	{
		*pg_rxput = 0;
	}
}

#endif /* SPIIF_H_ */
