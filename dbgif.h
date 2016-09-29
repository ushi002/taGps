/*
 * dbgif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef DBGIF_H_
#define DBGIF_H_
#include "typedefs.h"
#include "spiif.h"

#define MSG_SIZE MEM_PAGE_SIZE
#define ERR_MSG_LEN 	40

Boolean buff_empty(void);

void dbg_initport(void);
void dbg_inituart(void);

void dbg_txerrmsg(U8 err_code);
void dbg_txmsg(char * msg);
void dbg_txchar(U8 c);

void pcif_rxchar(void);
void pcif_enif(void);
void pcif_disif(void);

Boolean pcif_has_power(void);

extern U8 * pg_rxspi_txpc_buff;
extern U16 * pg_rxspi_txpc_put;
extern U16 * pg_rxspi_txpc_pop;

inline Boolean buff_empty(void)
{
	Boolean ret = false;

	if (*pg_rxspi_txpc_put == *pg_rxspi_txpc_pop)
	{
		ret = true;
	}
	return ret;
}


inline U8 buff_pop(void)
{
	U8 ret;

	ret = pg_rxspi_txpc_buff[*pg_rxspi_txpc_pop];
	(*pg_rxspi_txpc_pop)++;
	if (*pg_rxspi_txpc_pop == SPI_RX_SIZE)
	{
		*pg_rxspi_txpc_pop = 0;
	}

	return ret;
}

inline static void buff_put(U8 c)
{
	pg_rxspi_txpc_buff[*pg_rxspi_txpc_put] = c;
	(*pg_rxspi_txpc_put)++;
	if (*pg_rxspi_txpc_put == SPI_RX_SIZE)
	{
		*pg_rxspi_txpc_put = 0;
	}
}

#endif /* DBGIF_H_ */
