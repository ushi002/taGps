/*
 * ubxprot.h
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */

#ifndef UBXPROT_H_
#define UBXPROT_H_

#include "typedefs.h"

#define MAX_MESSAGEBUF_LEN    100

typedef enum MessageId_t
{
	MessageIdNone = 0,
	MessageIdPollCfgNmea = 1,
	MessageIdPollCfgPrt = 2,
	MessageIdSetCfgPrt = 3
}MessageId_e;

typedef enum BufferId_t
{
	BufferIdPollSetCfgNmea = 0,
	BufferIdPollSetCfgPrt = 1
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

typedef struct Message_t
{
	MessageId_e id;
	TypeOfConfirm_e confirmType;
	Boolean confirmed;
	U8 * pMsgBuff;
	UbxPckHeader_s * pHead;
}Message_s;

void ubx_init(void);
const Message_s * ubx_get_msg(MessageId_e msgId);
void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb);
U16 ubx_poll_cfgnmea(U8 * msg);
U16 ubx_poll_cfgprt(U8 * msg);
U16 ubx_set_cfgprt(U8 * msg);

U16 ubx_checkmsg(U8 * msg);
void ubx_msgst(const Message_s * pLastMsg, const U8 * pNewMsg);
void ubx_msg_polled(const Message_s * pLastMsg, const U8 * pNewMsgData);
void ubx_msg_confirmed(const Message_s * pLastMsg);
#endif /* UBXPROT_H_ */
