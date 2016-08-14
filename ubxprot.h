/*
 * ubxprot.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef UBXPROT_H_
#define UBXPROT_H_

#include "typedefs.h"

#define MAX_MESSAGEBUF_LEN    150
#define CFG_GNSS_BLOCKS		5

typedef enum MessageId_t
{
	MessageIdNone = 0,
	MessageIdPollCfgNmea = 1,
	MessageIdPollCfgPrt = 2,
	MessageIdSetCfgPrt = 3,
	MessageIdPollPvt = 4,
	MessageIdPollCfgGnss = 5,
	MessageIdSetCfgGnss = 6,
	MessageIdPollCfgPm2 = 7,
	MessageIdSetCfgPm2 = 8,
}MessageId_e;

typedef enum BufferId_t
{
	BufferIdPollSetCfgNmea = 0,
	BufferIdPollSetCfgPrt = 1,
	BufferIdPollPvt = 2,
	BufferIdPollSetCfgGnss = 3,
	BufferIdPollSetCfgPm2 = 4,
}BufferId_e;

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

typedef enum TypeOfConfirm_t
{
	TypeOfConfirmNone = 0,
	/** Brief Ubx input message is acknowledged */
	TypeOfConfirmAck = 1,
	/** Brief Ubx polling request generates a message */
	TypeOfConfirmMsg = 2
}TypeOfConfirm_e;

typedef enum UbxClassIdAck_t
{
	UbxClassIdAckAck = 0x01,
	ubxClassIdAckNack = 0x00
}UbxClassIdAck_e;

typedef enum UbxClassIdCfg_t
{
	UbxClassIdAck = 0x05,
	UbxClassIdCfgPrt = 0x00,
	UbxClassIdCfgNmea = 0x17,
	UbxClassIdCfgGnss = 0x3e,
	UbxClassIdCfgPm2 = 0x3b,
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

typedef struct UbxAckMsg_t
{
	U8  ackMsgCls;
	U8  ackMsgId;
}UbxAckMsg_s;

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

typedef struct GnssBlock_t
{
    U8  gnssId;
    U8  resTrkCh;
    U8  maxTrkCh;
    U8  reserved1;
    U32 flags;
}GnssBlock_s;

typedef struct UbxCfgGnss_t
{
    U8  msgVer;
    U8  numTrkChHw;
    U8  numTrkChUse;
    U8  numConfigBlocks;
    //for ublox-m8c we have 5 blocks:
    GnssBlock_s gnssBlock[CFG_GNSS_BLOCKS];
}UbxCfgGnss_s;

typedef struct UbxCfgPm2_t
{
    U8  version;
    U8  reserved1;
    U8  maxStartupStateDur;
    U8  reserved2;
    U32 flags;
    U32 updatePeriod;
    U32 searchPeriod;
    U32 gridOffset;
    U16 onTime;
    U16 minAcqTime;
    U8  reserved3[20];
}UbxCfgPm2_s;

typedef struct UbxNavPvt_t
{
    U32 iTOW;
    U16 year;
    U8  month;
    U8  day;
    U8  hour;
    U8  min;
    U8  sec;
    U8  valid;
    U32 tAcc;
    I32 nano;
    /* Brief Fixtype: - 0x00 = No Fix
     * 		- 0x01 = Dead Reckoning only
     * 		- 0x02 = 2D-Fix
     * 		- 0x03 = 3D-Fix
     * 		- 0x04 = GNSS + dead reckoning combined
     * 		- 0x05 = Time only fix  */
    U8  fixType;
    U8  flags;
    U8  reserved1;
    U8  numSV;
    I32 lon;
    I32 lat;
    I32 height;
    I32 hMSL;
    U32 hAcc;
    U32 vAcc;
    I32 velN;
    I32 velE;
    I32 velD;
    I32 gSpeed;
    I32 headMot;
    U32 sAcc;
    U32 headAcc;
    U16 pDOP;
    U16 reserved2a;
    U32 reserved2b;
    I32 HeadVeh;
    U32 reseverd3;
}UbxNavPvt_s;

typedef union MessageBody_t
{
	UbxCfgPrt_s cfgPrt;
	UbxCfgGnss_s cfgGnss;
	UbxCfgPm2_s cfgPm2;
	UbxNavPvt_s navPvt;
}MessageBody_u;

typedef struct Message_t
{
	MessageId_e id;
	TypeOfConfirm_e confirmType;
	Boolean confirmed;
	U8 * pMsgBuff;
	UbxPckHeader_s * pHead;
	MessageBody_u * pBody;
}Message_s;

void ubx_init(void);
const Message_s * ubx_get_msg(MessageId_e msgId);
void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb);
U16 ubx_poll_cfgnmea(U8 * msg);
U16 ubx_poll_cfgprt(U8 * msg);
U16 ubx_set_cfgprt(U8 * msg);
U16 ubx_poll_cfggnss(U8 * msg);
U16 ubx_set_cfggnss(U8 * msg);
U16 ubx_poll_cfgpm2(U8 * msg);
U16 ubx_set_cfgpm2(U8 * msg);
U16 ubx_poll_pvt(U8 * msg);

U16 ubx_checkmsg(U8 * msg);
void ubx_msgst(const Message_s * pLastMsg, const U8 * pNewMsg);
void ubx_msg_polled(const Message_s * pLastMsg, const U8 * pNewMsgData);
void ubx_msg_confirmed(const Message_s * pLastMsg);
#endif /* UBXPROT_H_ */
