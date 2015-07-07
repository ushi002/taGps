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



void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb);
U8 ubx_poll_cfgnmea(U8 * msg);

U16 ubx_procmsg(U8 * msg);
#endif /* UBXPROT_H_ */
