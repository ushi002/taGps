/*
 * dbgif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef DBGIF_H_
#define DBGIF_H_
#include "typedefs.h"

#define ERR_MSG_LEN 	40

U8 buff_getch(void);
Boolean buff_empty(void);

void dbg_initport(void);
void dbg_inituart(void);

void dbg_txchar(U8 * pChar);
void dbg_txerrmsg(U8 err_code);
void dbg_txmsg(char * msg);

void pcif_rxchar(void);
void pcif_enif(void);
void pcif_disif(void);

#endif /* DBGIF_H_ */
