/*
 * dbgif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef DBGIF_H_
#define DBGIF_H_
#include "typedefs.h"

#define MSG_SIZE 128
#define ERR_MSG_LEN 	40

Boolean buff_empty(void);

void dbg_initport(void);
void dbg_inituart(void);

void dbg_txchar(U8 * pChar);
void dbg_txerrmsg(U8 err_code);
void dbg_txmsg(char * msg);

void pcif_rxchar(void);
void pcif_enif(void);
void pcif_disif(void);


extern U8 * gp_txbuff;
extern U8 * pg_txbput;
extern U8 * pg_txbpop;

inline Boolean buff_empty(void)
{
	Boolean ret = false;

	if (*pg_txbput == *pg_txbpop)
	{
		ret = true;
	}
	return ret;
}


inline U8 buff_pop(void)
{
	U8 ret;

	ret = gp_txbuff[*pg_txbpop];
	(*pg_txbpop)++;
	if (*pg_txbpop == MSG_SIZE)
	{
		*pg_txbpop = 0;
	}

	return ret;
}

#endif /* DBGIF_H_ */
