/*
 * ubxprot.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef UBXPROT_H_
#define UBXPROT_H_

#include "typedefs.h"

typedef enum UbxClass_t
{
	ubxClassNav = 0x01,
	ubxClassRxm = 0x02,
	ubxClassInf = 0x04,
	ubxClassAck = 0x05,
	ubxClassCfg = 0x06,
	ubxClassUpd = 0x09,
	ubxClassMon = 0x0a,
	ubxClassAid = 0x0b,
	ubxClassTim = 0x0d,
	ubxClassMga = 0x13,
	ubxClassLog = 0x21
}UbxClass_e;

typedef enum UbxClassIdAck_t
{
	UbxClassIdAckAck = 0x01,
	ubxClassIdAckNack = 0x00
}UbxClassIdAck_e;

typedef enum UbxClassIdCfg_t
{
	UbxClassIdCfgPrt = 0x00,
	UbxClassIdCfgNmea = 0x17
}UbxClassIdCfg_e;

typedef struct UbxPckHeader_t
{
	U8  syncChars[2];
	U8  ubxClass;
	U8  ubxId;
	U16 length;
}UbxPckHeader_s;

typedef struct UbxPckChecksum_t
{
	U8  cka;
	U8  ckb;
}UbxPckChecksum_s;

typedef struct UbxCfgPrt_t
{
    U8  portId;
    U8  reserved;
    U16 txReady;
    U32 mode;
    U32 baudRate;
    U16 inProtoMask;
    U16 outProtoMask;
    U16 flags;
    U16 reserved2;
}UbxCfgPrt_s;


void ubx_init(void);
void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb);
U8 ubx_poll_cfgnmea(U8 * msg);
U8 ubx_poll_cfgprt(U8 * msg);
U8 ubx_set_cfgprt(U8 * msg);

U16 ubx_checkmsg(U8 * msg);
void ubx_msgst(U8 * pMsg);
#endif /* UBXPROT_H_ */
