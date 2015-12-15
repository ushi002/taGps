/*
 * gpsif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef GPSIF_H_
#define GPSIF_H_
#include "typedefs.h"
#include "ubxprot.h"

typedef enum{
	ubxstat_idle = 0,
	ubxstat_syncchar2 = 1,
	ubxstat_msgclass = 2,
	ubxstat_msgid = 3,
	ubxstat_getlen = 4,
	ubxstat_rec = 5
} ubxstat_e;

void gps_initport(void);
void gps_inituart(void);
void gps_uart_ie(void);
void gps_uart_id(void);

void gps_cmdtx(U8 * buff);
U16 gps_rx_ubx_msg(const Message_s * lastMsg);

#endif /* GPSIF_H_ */
