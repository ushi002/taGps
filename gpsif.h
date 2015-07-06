/*
 * gpsif.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef GPSIF_H_
#define GPSIF_H_

void gps_initport(void);
void gps_inituart(void);

void gps_cmdtx(U8 * buff, U16 len);

#endif /* GPSIF_H_ */
