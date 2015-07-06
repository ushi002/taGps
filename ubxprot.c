/*
 * ubxprot.c
 *
 *  Created on: Jul 6, 2015
 *      Author: ludek
 */


#include "ubxprot.h"

U8 ubx_poll_cfgnmea(U8 * msg)
{
	UbxPckHeader_s * pHead;
	UbxPckChecksum_s * pCheckSum;
	U8 * buffForChecksum;
	U16 checksumByteRange;

	pHead = (UbxPckHeader_s *) msg;

	pHead->ubxClass = ubxClassCfg;
	pHead->ubxId = UbxClassIdCfgNmea;
	pHead->length = 0;

	pCheckSum = (UbxPckChecksum_s *) (msg + sizeof(UbxPckHeader_s) + pHead->length);

	checksumByteRange = pHead->length + 4; //4 stays for class, ID, length field
	buffForChecksum = (U8 *) &pHead->ubxClass;

	ubx_genchecksum(buffForChecksum, checksumByteRange, &pCheckSum->cka, &pCheckSum->ckb);

	return 8; //length of UBX_CFG_NMEA poll request
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
