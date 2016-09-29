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

#define TXB_SIZE 	MAX_MESSAGEBUF_LEN

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
void gps_ie(void);
void gps_id(void);
void gps_uart_enable(void);
void gps_uart_disable(void);

void gps_pulse_dis(void);
void gps_pulse_en(void);

void gps_initcmdtx(U8 * buff);
void gps_cmdtx(U8 * buff);
U16 gps_rx_ubx_msg(const Message_s * lastMsg, Boolean interruptCall);
Boolean gps_has_power(void);

extern U8 * pgps_txbuf;
extern U8 * pgps_txput;
extern U8 * pgps_txpop;
extern Boolean * pgGpsTxInProgress;

/** @brief Set flag when transmitting gps command */
static inline void gps_set_txbusy(Boolean transmitting)
{
	*pgGpsTxInProgress = transmitting;
}
/** @brief Get flag of the gps command transmission */
static inline Boolean gps_get_txbusy(void)
{
	return *pgGpsTxInProgress;
}

inline Boolean gps_txempty(void)
{
	Boolean ret = false;

	if (*pgps_txput == *pgps_txpop)
	{
		ret = true;
	}
	return ret;
}

inline U8 gps_txbpop(void)
{
	U8 ret;

	ret = pgps_txbuf[*pgps_txpop];
	(*pgps_txpop)++;
	if (*pgps_txpop == TXB_SIZE)
	{
		*pgps_txpop = 0;
	}
	return ret;
}

#endif /* GPSIF_H_ */
