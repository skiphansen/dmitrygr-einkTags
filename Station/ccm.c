#include <string.h>
#include "aes.h"
#include "ccm.h"

//a generic and working implementation of AES-CCM is provided in the comment
// below, the version given here is hardcoded for the params mentioned in ccm.h,
// and for architectures with unaligned access


static void aesCcmPrvCopyNonceSetHalfword(uint8_t *dst, const void *nonce, uint32_t val)
{
	*(uint32_t*)(dst + 0) = ((uint32_t*)nonce)[0];
	*(uint32_t*)(dst + 4) = ((uint32_t*)nonce)[1];
	*(uint32_t*)(dst + 8) = ((uint32_t*)nonce)[2];
	dst[12] = ((uint8_t*)nonce)[12];
	dst[13] = val >> 8;
	dst[14] = val;
}

static uint32_t aesCcmPrvCalcUnencryptedMic(const uint8_t *encSrc, uint32_t encSrcLen, const uint8_t *authSrc, uint16_t authSrcLen, const uint8_t *key, const uint8_t *nonce)
{
	uint8_t blockIn[16], blockOut[16];
	uint32_t done, now;
	uint_fast8_t i;
	
	//create block 0
	blockIn[0] = (authSrcLen ? 0x40 : 0x00) | 0x09;
	aesCcmPrvCopyNonceSetHalfword(blockIn + 1, nonce, encSrcLen);
	
	//encrypt it
	aesEnc(key, blockIn, blockOut);

	if (authSrcLen) {

		uint32_t already = 2;		//account for the prepended length
		
		blockIn[0] = authSrcLen >> 8;
		blockIn[1] = authSrcLen;
		already = 2;
		done = 0;
		
		while (done < authSrcLen) {
			
			now = 16 - already;
			if (now > authSrcLen - done)
				now = authSrcLen - done;
			
			memcpy(blockIn + already, authSrc + done, now);
			memset(blockIn + already + now, 0, 16 - already - now);
			already = 0;
			done += now;
			
			for (i = 0; i < 16 / sizeof(uint32_t); i++)
				((uint32_t*)blockIn)[i] ^= ((uint32_t*)blockOut)[i];
			
			aesEnc(key, blockIn, blockOut);
		}
	}
	
	done = 0;
	while (done < encSrcLen) {
		
		now = 16;
		if (now > encSrcLen - done)
			now = encSrcLen - done;
		
		memcpy(blockIn, encSrc + done, now);
		memset(blockIn + now, 0, 16 - now);
		done += now;
		
		for (i = 0; i < 16 / sizeof(uint32_t); i++)
			((uint32_t*)blockIn)[i] ^= ((uint32_t*)blockOut)[i];
		
		aesEnc(key, blockIn, blockOut);
	}
	
	//return the unencrypted MIC
	return *(uint32_t*)blockOut;
}

void aesCcmEnc(void* dstP, const void *src, uint16_t encSrcLen, const void *authSrc, uint16_t authSrcLen, const void *key, const void *nonce)
{
	uint8_t blockIn[16], blockOut[16], *dst = (uint8_t*)dstP;
	const uint8_t *encSrc = (const uint8_t*)src;
	uint32_t done, now, ctr = 0, mic;
	uint_fast8_t i;
	
	//it goes after encrypted data
	mic = aesCcmPrvCalcUnencryptedMic(src, encSrcLen, authSrc, authSrcLen, key, nonce);

	//now we encrypt
	done = 0;
	now = 0;	//first block not used
	while (done < encSrcLen) {
		
		if (now > encSrcLen - done)
			now = encSrcLen - done;
		
		blockIn[0] = 1;
		aesCcmPrvCopyNonceSetHalfword(blockIn + 1, nonce, ctr++);
		
		aesEnc(key, blockIn, blockOut);
		
		if (!now) {	//first block
			
			mic ^= *(uint32_t*)blockOut;
		}
		else {
			
			for (i = 0; i < now; i++)
				dst[done + i] = *encSrc++ ^ blockOut[i];
		}
		done += now;
		now = 16;
	}
	
	*(uint32_t*)(dst + encSrcLen) = mic;
}

bool aesCcmDec(void* dstP, const void *srcP, uint16_t encDataLen, const void *authSrc, uint16_t authSrcLen, const void *key, const void *nonce)
{
	uint32_t done, now, ctr = 0, micGiven = 0;
	uint8_t blockIn[16], blockOut[16], *dst = (uint8_t*)dstP;
	const uint8_t *src = (const uint8_t*)srcP;
	uint_fast8_t i;
	
	//remember: MIC is after data
		
	//first we decrypt
	done = 0;
	now = 0;	//first block not used
	while (done < encDataLen) {
		
		if (now > encDataLen - done)
			now = encDataLen - done;
		
		blockIn[0] = 1;
		aesCcmPrvCopyNonceSetHalfword(blockIn + 1, nonce, ctr++);
		
		aesEnc(key, blockIn, blockOut);
		
		if (!now) {	//first block
			
			micGiven = *(uint32_t*)(src + encDataLen) ^ *(uint32_t*)blockOut;
		}
		else {
			
			for (i = 0; i < now; i++)
				dst[done + i] = *src++ ^ blockOut[i];
		}
		done += now;
		
		now = 16;
	}
	return micGiven == aesCcmPrvCalcUnencryptedMic(dst, encDataLen, authSrc, authSrcLen, key, nonce);
}











/*	//				GENERIC VERSION

//CCM config
	static const uint32_t t = 4;		// MIC len in bytes. only even numbers 4..16 allowed
	static const uint32_t q = 2;		// num bytes to represent data length: 2..8
	//static const uint32_t n = 15 - q;	// nonce length 7..13. n + q MUST == 15
	static const uint32_t n = 13;	// nonce length 7..13. n + q MUST == 15
	

static void aesCcmCalcUnencryptedMic(uint8_t *dst, const uint8_t *encSrc, uint32_t encSrcLen, const uint8_t *authSrc, uint16_t authSrcLen, const uint8_t *key, const uint8_t *nonce)
{
	uint8_t blockIn[16], blockOut[16];
	uint32_t done, now, ctr = 0;
	uint_fast8_t i;
	
	//authSrcLen must be < 0xff00 to be complaint!
	
	//create block 0
	blockIn[0] = (authSrcLen ? 0x40 : 0x00) | (((t - 2) / 2) << 3) | (q - 1);
	memcpy(blockIn + 1, nonce, n);
	for (i = 0; i < q; i++)		//BE
		blockIn[1 + n + i] = encSrcLen >> (8 * (q - i - 1));
	
	//encrypt it
	aesEnc(key, blockIn, blockOut);

	if (authSrcLen) {

		uint32_t already = 2;		//account for the prepended length
		
		blockIn[0] = authSrcLen >> 8;
		blockIn[1] = authSrcLen;
		already = 2;
		done = 0;
		
		while (done < authSrcLen) {
			
			now = 16 - already;
			if (now > authSrcLen - done)
				now = authSrcLen - done;
			
			memcpy(blockIn + already, authSrc + done, now);
			memset(blockIn + already + now, 0, 16 - already - now);
			already = 0;
			done += now;
			
			for (i = 0; i < 16; i++)
				blockIn[i] ^= blockOut[i];
			
			aesEnc(key, blockIn, blockOut);
		}
	}
	
	done = 0;
	while (done < encSrcLen) {
		
		now = 16;
		if (now > encSrcLen - done)
			now = encSrcLen - done;
		
		memcpy(blockIn, encSrc + done, now);
		memset(blockIn + now, 0, 16 - now);
		done += now;
		
		for (i = 0; i < 16; i++)
			blockIn[i] ^= blockOut[i];
		
		aesEnc(key, blockIn, blockOut);
	}
	
	//copy out the almost-complete MIC
	memcpy(dst, blockOut, t);
}

static void aesCcmEnc(uint8_t* dst, const uint8_t *encSrc, uint32_t encSrcLen, const uint8_t *authSrc, uint16_t authSrcLen, const uint8_t *key, const uint8_t *nonce)
{
	uint8_t blockIn[16], blockOut[16];
	uint32_t done, now, ctr = 0;
	uint_fast8_t i;
	
	//it goes after encrypted data
	aesCcmCalcUnencryptedMic(dst + encSrcLen, encSrc, encSrcLen, authSrc, authSrcLen, key, nonce);

	//now we encrypt
	done = 0;
	now = 0;	//first block not used
	while (done < encSrcLen) {
		
		if (now > encSrcLen - done)
			now = encSrcLen - done;
		
		blockIn[0] = q - 1;
		memcpy(blockIn + 1, nonce, n);
		
		for (i = 0; i < q; i++)		//BE
			blockIn[1 + n + i] = ctr >> (8 * (q - i - 1));
		ctr++;
		
		aesEnc(key, blockIn, blockOut);
		
		if (!now) {	//first block
						
			for (i = 0; i < t; i++)
				dst[encSrcLen + i] ^= blockOut[i];
		}
		else {
			
			for (i = 0; i < now; i++)
				dst[done + i] = *encSrc++ ^ blockOut[i];
		}
		done += now;
		
		now = 16;
	}
}

static bool aesCcmDec(uint8_t* dst, const uint8_t *encSrc, uint32_t encSrcLen, const uint8_t *authSrc, uint16_t authSrcLen, const uint8_t *key, const uint8_t *nonce)
{
	uint8_t blockIn[16], blockOut[16], micGiven[t], micCalced[t];
	uint32_t done, now, ctr = 0, encDataLen = encSrcLen - t;
	uint_fast8_t i;
	
	//data must be at least as long as the MIC
	if (encSrcLen < t)
		return false;
		
	//first we decrypt
	done = 0;
	now = 0;	//first block not used
	while (done < encDataLen) {
		
		if (now > encDataLen - done)
			now = encDataLen - done;
		
		blockIn[0] = q - 1;
		memcpy(blockIn + 1, nonce, n);
		
		for (i = 0; i < q; i++)		//BE
			blockIn[1 + n + i] = ctr >> (8 * (q - i - 1));
		ctr++;
		
		aesEnc(key, blockIn, blockOut);
		
		if (!now) {	//first block
						
			for (i = 0; i < t; i++)
				micGiven[i] = encSrc[encDataLen + i] ^ blockOut[i];
		}
		else {
			
			for (i = 0; i < now; i++)
				dst[done + i] = *encSrc++ ^ blockOut[i];
		}
		done += now;
		
		now = 16;
	}
		
	//it goes after encrypted data
	aesCcmCalcUnencryptedMic(micCalced, dst, encDataLen, authSrc, authSrcLen, key, nonce);

	return !memcmp(micCalced, micGiven, t);
}

*/