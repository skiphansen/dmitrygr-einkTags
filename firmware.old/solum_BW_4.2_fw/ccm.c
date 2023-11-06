#include <string.h>
#include "mz100.h"
#include "ccm.h"


static bool aesCcmOp(void* dst, const void *src, uint16_t authSrcLen, uint16_t encSrcLen, const void *key, const void *nonce, bool dec)
{
	uint32_t tempB, nBytesNoMic = authSrcLen + encSrcLen, nBytesIn, nBytesOut;
	const uint32_t *inD = (const uint32_t*)src;
	uint32_t *outD = (uint32_t*)dst;
	uint_fast8_t i;
	bool success;
	
	if (dec) {
		nBytesIn = nBytesNoMic + AES_CCM_MIC_SIZE;
		nBytesOut = nBytesNoMic;
	}
	else {
		nBytesIn = nBytesNoMic;
		nBytesOut = nBytesNoMic + AES_CCM_MIC_SIZE;
	}
	
	do{
		AES->CTRL1 |= 2;
	} while (!(AES->STATUS & 2));
	
	AES->CTRL2 |= 1;
	(void)AES->CTRL2;
	AES->CTRL2 &=~ 1;
	AES->CTRL1 = 0x0005501e + (dec ? 0x8000 : 0);
	AES->IMR = 0xffffffff;
	
	for(i = 0; i < 4; i++)
		AES->KEY[7 - i] = ((uint32_t*)key)[i];
	
	AES->MSTR_LEN = encSrcLen;
	AES->ASTR_LEN = authSrcLen;
	
	for(i = 0; i < 3; i++)
		AES->IV[i] = ((uint32_t*)nonce)[i];
	AES->IV[3] = ((uint8_t*)nonce)[12] + 0x200;	//2 byte lengths
	
	AES->CTRL1 |= 1;
	
	while (nBytesIn || nBytesOut) {
		
		if (!(AES->STATUS & 0x10) && nBytesIn) {
			
			if (nBytesIn >= 4) {
				AES->STR_IN = *inD++;
				nBytesIn -= 4;
			}
			else {
				memcpy(&tempB, inD, nBytesIn);
				AES->STR_IN = tempB;
				nBytesIn = 0;
			}
		}
		
		if ((AES->STATUS & 0x40) && nBytesOut) {
			
			if (nBytesOut >= 4) {
				*outD++ = AES->STR_OUT;
				nBytesOut -= 4;
			}
			else {
				tempB = AES->STR_OUT;
				memcpy(outD, &tempB, nBytesOut);
				nBytesOut = 0;
			}
			
		}
	}
	
	success = !((AES->STATUS >> 11) & 7);
	
	AES->CTRL1 = 0;
	
	return success;
}

void aesCcmEnc(void* dst, const void *src, uint16_t authSrcLen, uint16_t encSrcLen, const void *key, const void *nonce)
{
	aesCcmOp(dst, src, authSrcLen, encSrcLen, key, nonce, false);
}

bool aesCcmDec(void* dst, const void *src, uint16_t authSrcLen, uint16_t encSrcLen, const void *key, const void *nonce)
{
	return aesCcmOp(dst, src, authSrcLen, encSrcLen, key, nonce, true);
}




