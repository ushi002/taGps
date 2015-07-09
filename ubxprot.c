/*
 * ubxprot.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */


#include "ubxprot.h"
#include "dbgif.h"

void ubx_addchecksum(U8 * msg);

#define MAX_MESSAGES_NUM    10
#define MAX_MESSAGE_LEN     40

static U8 ubx_messages[MAX_MESSAGES_NUM][MAX_MESSAGE_LEN];

void ubx_init(void)
{
    I16 i, k;

    for (i = MAX_MESSAGES_NUM-1; i >= 0; i--)
    {
        for (k = MAX_MESSAGE_LEN-1; k >= 0; k--)
        {
            ubx_messages[i][k] = 0;
        }
    }
}

U16 ubx_poll_cfgnmea(U8 * msg)
{
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) msg;

	pHead->ubxClass = ubxClassCfg;
	pHead->ubxId = UbxClassIdCfgNmea;
	pHead->length = 0;

	ubx_addchecksum(msg);

	return 0;
}

U16 ubx_poll_cfgprt(U8 * msg)
{
	UbxPckHeader_s * pHead;

	pHead = (UbxPckHeader_s *) msg;

	pHead->ubxClass = ubxClassCfg;
	pHead->ubxId = UbxClassIdCfgPrt;
	pHead->length = 0;

	ubx_addchecksum(msg);

	return 0;
}


U16 ubx_set_cfgprt(U8 * msg)
{
    I16 i, k;
    U16 message_found = 0;
    UbxPckHeader_s * pHead;
    UbxCfgPrt_s *pBody;
    U16 retVal = 0;

    //find existing cfgprt message:
    for (i = MAX_MESSAGES_NUM-1; i >= 0; i--)
    {
        if (ubx_messages[i][2] == ubxClassCfg &&
                ubx_messages[i][3] == UbxClassIdCfgPrt) //message found, use it
        {
            message_found = 1;
            pHead = (UbxPckHeader_s *) ubx_messages[i];
            for (k = pHead->length+8-1; k >= 0; k--)
            {
                msg[k] = ubx_messages[i][k];
            }
            break;
        }
    }
    if (!message_found)
    {
        dbg_lederror();
        dbg_txerrmsg(3);
        retVal = 1;
    }

    pBody = (UbxCfgPrt_s *) (msg + sizeof(UbxPckHeader_s));
    pBody->outProtoMask = 1; //keep only UBX messages, disable NMEA

    ubx_addchecksum(msg);

    return retVal;
}

/** @brief Add U-BLOX checksum to the end of message */
void ubx_addchecksum(U8 * msg)
{
    UbxPckHeader_s * pHead;
    UbxPckChecksum_s * pCheckSum;
    U8 * buffForChecksum;
    U16 checksumByteRange;

    pHead = (UbxPckHeader_s *) msg;

    pCheckSum = (UbxPckChecksum_s *) (msg + sizeof(UbxPckHeader_s) + pHead->length);

    checksumByteRange = pHead->length + 4; //4 stays for class, ID, length field
    buffForChecksum = (U8 *) &pHead->ubxClass;

    ubx_genchecksum(buffForChecksum, checksumByteRange, &pCheckSum->cka, &pCheckSum->ckb);
}

/** @brief Compute U-BLOX proprietary protocol checksum
 *
 * @param pBuff pointer to the buffer over which is the
 * checksum computed
 * @param len length of the input buffer
 * @param pCka pointer to the first byte of the checksum
 * @param pCkb pointer to the second byte of the checksum */
void ubx_genchecksum(const U8 * pBuff, U16 len, U8 * pCka, U8 * pCkb)
{
	U16 i;

	*pCka = 0;
	*pCkb = 0;

	for(i = 0; i < len; i++)
	{
		*pCka = *pCka + pBuff[i];
		*pCkb = *pCkb + *pCka;
	}
}

/** @brief Process U-BLOX proprietary protocol message
 *
 * @param msg pointer to UBX message */
U16 ubx_checkmsg(U8 * pMsg)
{
	UbxPckHeader_s * pMsgHead;
	UbxPckChecksum_s * pMsgCs;
	UbxPckChecksum_s cs;
	U8 * buffForChecksum;
	U16 checksumByteRange;

	U16 retVal = 0;

	pMsgHead = (UbxPckHeader_s *) pMsg;
	pMsgCs = (UbxPckChecksum_s *) (pMsg + sizeof(UbxPckHeader_s) + pMsgHead->length);

	checksumByteRange = pMsgHead->length + 4; //4 stays for class, ID, length field
	buffForChecksum = (U8 *) &pMsgHead->ubxClass;

	ubx_genchecksum(buffForChecksum, checksumByteRange, &cs.cka, &cs.ckb);

	if ((pMsgCs->cka != cs.cka) || (pMsgCs->cka != cs.cka))
	{
		retVal = 1;
	}

	return retVal;
}

void ubx_msgst(U8 * pMsg)
{
    I16 i, k;
    UbxPckHeader_s * pHead = (UbxPckHeader_s *) pMsg;
    U16 message_found = 0;

    //search for first free message
    for (i = MAX_MESSAGES_NUM-1; i >= 0; i--)
    {
        if (ubx_messages[i][2] == pHead->ubxClass &&
                ubx_messages[i][3] == pHead->ubxId) //the same message found, update it
        {
            message_found = 1;
            for (k = pHead->length+8-1; k >= 0; k--)
            {
                ubx_messages[i][k] = pMsg[k];
            }
            break;
        }
    }
    if (!message_found)
    {
        for (i = MAX_MESSAGES_NUM-1; i >= 0; i--) //search for a free space
        {

            if (ubx_messages[i][0] == 0) //empty field, store message
            {
                message_found = 1;
                for (k = pHead->length+8-1; k >= 0; k--)
                {
                    ubx_messages[i][k] = pMsg[k];
                }
                break;
            }
        }
    }
    if (!message_found) //no matching message, no more free space...
    {
        dbg_lederror();
        dbg_txerrmsg(2);
    }
}
