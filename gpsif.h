/*
 * gpsif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef GPSIF_H_
#define GPSIF_H_
#include "typedefs.h"

typedef enum{
	ubxstat_idle = 0,
	ubxstat_started = 1,
	ubxstat_getlen = 2,
	ubxstat_rec = 3,
	ubxstat_process = 4
} ubxstat_e;

void gps_initport(void);
void gps_inituart(void);

void gps_cmdtx(U8 * buff, U16 len);
U16 gps_rxchar(void);

#endif /* GPSIF_H_ */
