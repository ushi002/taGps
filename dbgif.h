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


void dbg_initport(void);
void dbg_inituart(void);

void dbg_txchar(U8 * pChar);
void dbg_txerrmsg(U8 err_code);
void dbg_txmsg(char * msg);
void dbg_ledok(void);
void dbg_lederror(void);
void dbg_ledsoff(void);

#endif /* DBGIF_H_ */
