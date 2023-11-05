#include "asmUtil.h"
#include "printf.h"
#include "cc111x.h"
#include "ccm.h"

#define AES_KEY_SIZE		16
#define AES_BLOCK_SIZE		16

//TI does not provide nearly enough docs to get AES-CCM working on the CC1110. Le Sigh...


static uint8_t __xdata mBlockOut[16];
static uint8_t __xdata mBlockIn[16];
static uint8_t __xdata mMic[4];

#pragma callee_saves aesPrvSetKey
static void aesPrvSetKey(const uint8_t __xdata *key)
{
	uint8_t i;
	
	//upload key
	ENCCS = 0x05;
	for (i = 0; i < AES_KEY_SIZE; i++)
		ENCDI = *key++;
	while (!(ENCCS & 0x08));
}

#pragma callee_saves aesPrvEnc
static void aesPrvEnc(void)		//	mBlockIn -> mBlockOut
{
	const uint8_t __xdata *src = mBlockIn;
	uint8_t __xdata *dst = mBlockOut;
	uint8_t i;
	
	ENCCS = 0x41;
	
	for (i = 0; i < AES_BLOCK_SIZE; i++)
		ENCDI = *src++;
	
	__asm__(
		"	push ar0				\n"
		"	mov  r0, #22			\n"
		"000001$:					\n"
		"	djnz r0, 000001$		\n"
		"	pop  ar0				\n"
	);
	
	for (i = 0; i < AES_BLOCK_SIZE; i++)
		*dst++ = ENCDO;
	
	while (!(ENCCS & 0x08));
}

//sdcc cannot inline things so we do for it
#define aesCcmPrvCopyNonceSetHalfword(_firstByte, nonce, _val)			\
do {																	\
	mBlockIn[0] = (_firstByte);											\
	xMemCopy(mBlockIn + 1, nonce, 13);									\
	mBlockIn[14] = 0;		/* normally val.hi, but for us - zero */	\
	mBlockIn[15] = (_val);												\
} while(0)

//leaves result in mBlockOut
static void aesCcmPrvCalcUnencryptedMic(const uint8_t __xdata *src, const struct AesCcmInfo __xdata *ccmInfo)
{
	uint8_t i, done, now;
	
	//create block 0
	aesCcmPrvCopyNonceSetHalfword(ccmInfo->authSrcLen ? 0x49 : 0x09, ccmInfo->nonce, ccmInfo->encDataLen);
	
	//encrypt it
	aesPrvEnc();

	if (ccmInfo->authSrcLen) {

		uint8_t __xdata *blk = mBlockIn;
		uint8_t already = 2;
		
		*blk++ = 0;		//authSrcLen.hi
		*blk++ = ccmInfo->authSrcLen;
		now = 14;	//since we already used 2
		done = 0;
		
		while (done < ccmInfo->authSrcLen) {
			
			if (now > (uint8_t)(ccmInfo->authSrcLen - done))
				now = (uint8_t)(ccmInfo->authSrcLen - done);
			
			xMemCopy(blk, src, now);
			src += now;
			xMemSet(blk + now, 0, 16 - already - now);
			
			for (i = 0; i < 16; i++)
				mBlockIn[i] ^= mBlockOut[i];
			
			aesPrvEnc();
			blk = mBlockIn;
			done += now;
			now = 16;
			already = 0;
		}
	}
	
	done = 0;
	while (done < ccmInfo->encDataLen) {
		
		now = 16;
		if (now > ccmInfo->encDataLen - done)
			now = ccmInfo->encDataLen - done;
		
		for (i = 0; i < now; i++)
			mBlockIn[i] = *src++ ^ mBlockOut[i];
		for (; i < 16; i++)
			mBlockIn[i] = mBlockOut[i];

		done += now;
		
		aesPrvEnc();
	}
}

void aesCcmEnc(void __xdata *dstP, const void __xdata *srcP, const struct AesCcmInfo __xdata *ccmInfo)
{
	const uint8_t __xdata *src = (const uint8_t __xdata*)srcP;
	uint8_t __xdata *dst = (uint8_t*)dstP;
	uint8_t i, done = 0, now, ctr = 0;
	
	aesPrvSetKey(ccmInfo->key);
	
	//it goes after encrypted data
	aesCcmPrvCalcUnencryptedMic(src, ccmInfo);
	xMemCopy(mMic, mBlockOut, sizeof(mMic));

	//copy authed data
	xMemCopy(dst, src, ccmInfo->authSrcLen);
	src += ccmInfo->authSrcLen;
	dst += ccmInfo->authSrcLen;

	//now we encrypt
	now = 0;	//first block not used
	while (done < ccmInfo->encDataLen) {
		
		if (now > (uint8_t)(ccmInfo->encDataLen - done))
			now = (uint8_t)(ccmInfo->encDataLen - done);
		
		aesCcmPrvCopyNonceSetHalfword(1, ccmInfo->nonce, ctr++);
		
		aesPrvEnc();
		
		if (!now)	//first block
			
			for (i = 0; i < sizeof(mMic); i++)
				mMic[i] ^= mBlockOut[i];
		else {
			
			for (i = 0; i < now; i++)
				*dst++ = *src++ ^ mBlockOut[i];
		}
		done += now;
		now = 16;
	}
	
	xMemCopy(dst, mMic, sizeof(mMic));
}

__bit aesCcmDec(void __xdata *dstP, const void __xdata *srcP, const struct AesCcmInfo __xdata *ccmInfo)
{
	const uint8_t __xdata *src = (const uint8_t __xdata*)srcP;
	uint8_t __xdata *dst = (uint8_t*)dstP;
	uint8_t i, done, now, ctr = 0;
	
	aesPrvSetKey(ccmInfo->key);
	
	//copy authed data
	xMemCopy(dst, src, ccmInfo->authSrcLen);
	src += ccmInfo->authSrcLen;
	dst += ccmInfo->authSrcLen;
	
	//then we decrypt
	done = 0;
	now = 0;	//first block not used
	while (done < ccmInfo->encDataLen) {
		
		if (now > (uint8_t)(ccmInfo->encDataLen - done))
			now = (uint8_t)(ccmInfo->encDataLen - done);
		
		aesCcmPrvCopyNonceSetHalfword(1, ccmInfo->nonce, ctr++);
		
		aesPrvEnc();
		
		if (!now) {	//first block
			
			//given mic is after data
			for (i = 0; i < sizeof(mMic); i++)
				mMic[i] = src[ccmInfo->encDataLen + i] ^ mBlockOut[i];
		}
		else {
			
			for (i = 0; i < now; i++)
				*dst++ = *src++ ^ mBlockOut[i];
		}
		done += now;
		now = 16;
	}
	
	aesCcmPrvCalcUnencryptedMic(dstP, ccmInfo);
	return xMemEqual(mMic, mBlockOut, sizeof(mMic));
}




