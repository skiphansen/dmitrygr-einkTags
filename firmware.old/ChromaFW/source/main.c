#define __packed
#include "proto.h"
#include "eepromMap.h"
#include "settings.h"
#include "asmUtil.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "cc111x.h"
#include "printf.h"
#include "eeprom.h"
#include "screen.h"
#include "radio.h"
#include "sleep.h"
#include "timer.h"
#include "comms.h"
#include "chars.h"
#include "ccm.h"
#include "adc.h"
#include "u1.h"

//#define PICTURE_FRAME_FLIP_EVERY_N_CHECKINS		24		//undefine to disable picture frame mode

#define COMPRESSION_BITPACKED_3x5_to_7	0x62700357	//3 pixels of 5 possible colors in 7 bits
#define COMPRESSION_BITPACKED_5x3_to_8	0x62700538	//5 pixels of 3 possible colors in 8 bits

static const uint64_t __code __at (0x008b) mVersion = 0x0000010100000001ull;
static uint8_t __xdata mRxBuf[COMMS_MAX_PACKET_SZ];
static struct Settings __xdata mSettings;
static uint32_t __pdata mTimerWaitStart;
static uint8_t __xdata mNumImgSlots;
static struct CommsInfo __xdata mCi;
static uint8_t __xdata mSelfMac[8];
int8_t __xdata mCurTemperature;


#define MAC_SECOND_WORD		0x44674b7aUL

struct BitmapFileHeader {
	uint8_t sig[2];
	uint32_t fileSz;
	uint8_t rfu[4];
	uint32_t dataOfst;
	uint32_t headerSz;			//40
	int32_t width;
	int32_t height;
	uint16_t colorplanes;		//must be one
	uint16_t bpp;
	uint32_t compression;
	uint32_t dataLen;			//may be 0
	uint32_t pixelsPerMeterX;
	uint32_t pixelsPerMeterY;
	uint32_t numColors;			//if zero, assume 2^bpp
	uint32_t numImportantColors;
	
};

struct BitmapClutEntry {
	uint8_t b, g, r, x;
};

struct EepromContentsInfo {
	uint32_t latestCompleteImgAddr, latestInprogressImgAddr, latestCompleteImgSize;
	uint64_t latestCompleteImgVer, latestInprogressImgVer;
	uint8_t numValidImages, latestImgIdx;
};

static void uiPrvDrawImageAtAddress(uint32_t addr);


static const char __xdata* fwVerString(void)
{
	static char __xdata fwVer[32];
	
	if (!fwVer[0]) {
		
		spr(fwVer, "FW v%u.%u.%*u.%*u",
			*(((uint8_t __code*)&mVersion) + 5),
			*(((uint8_t __code*)&mVersion) + 4),
			(uint16_t)(((uint8_t __code*)&mVersion) + 2),
			(uint16_t)(((uint8_t __code*)&mVersion) + 0)
		);
	}
	
	return fwVer;
}

static const char __xdata* macString(void)
{
	static char __xdata macStr[28];
	
	if (!macStr[0])
		spr(macStr, "%*M", (uint16_t)mSelfMac);
	
	return macStr;
}

static void prvEepromIndex(struct EepromContentsInfo __xdata *eci)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata*)mScreenRow;		//use screen buffer
	uint32_t __pdata addr = EEPROM_IMG_START;
	uint8_t slotId;
	
	xMemSet(eci, 0, sizeof(*eci));
	
	for (slotId = 0; slotId < mNumImgSlots; addr += EEPROM_IMG_EACH, slotId++) {
		
		static const uint32_t __code markerInProgress = EEPROM_IMG_INPROGRESS;
		static const uint32_t __code markerValid = EEPROM_IMG_VALID;
		
		uint32_t __xdata *addrP;
		uint64_t __xdata *verP;
		uint32_t __xdata *szP = 0;
		__bit isImage = false;
		
		eepromRead(addr, eih, sizeof(struct EepromImageHeader));
		pr("DATA slot %u (@0x%08lx): type 0x%*08lx ver 0x%*016llx\n",
			slotId, addr, (uint16_t)&eih->validMarker, (uint16_t)&eih->version);
		
		if (xMemEqual(&eih->validMarker, (void __xdata*)&markerInProgress, sizeof(eih->validMarker))) {
		
			verP = &eci->latestInprogressImgVer;
			addrP = &eci->latestInprogressImgAddr;
		}
		else if (xMemEqual(&eih->validMarker, (void __xdata*)&markerValid, sizeof(eih->validMarker))) {
			
			eci->numValidImages++;
			isImage = true;
			verP = &eci->latestCompleteImgVer;
			addrP = &eci->latestCompleteImgAddr;
			szP = &eci->latestCompleteImgSize;
		}
		else
			continue;
		
		if (!u64_isLt(&eih->version, verP)) {
			u64_copy(verP, &eih->version);
			*addrP = addr;
			if (szP)
				xMemCopy(szP, &eih->size, sizeof(eih->size));
			if (isImage)
				eci->latestImgIdx = slotId;
		}
	}
}

//similar to prvEepromIndex
static void uiPrvDrawNthValidImage(uint8_t n)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata*)mScreenRow;		//use screen buffer
	uint32_t __pdata addr = EEPROM_IMG_START;
	uint8_t slotId;
	
	for (slotId = 0; slotId < mNumImgSlots; addr += EEPROM_IMG_EACH, slotId++) {
		
		static const uint32_t __code markerValid = EEPROM_IMG_VALID;
		
		eepromRead(addr, eih, sizeof(struct EepromImageHeader));
		
		if (xMemEqual(&eih->validMarker, (void __xdata*)&markerValid, sizeof(eih->validMarker))) {
			
			if (!n--) {
				
				mSettings.lastShownImgSlotIdx = slotId;
				uiPrvDrawImageAtAddress(addr);
				return;
			}
		}
	}
}

#pragma callee_saves clockingInit
static void clockingInit(void)
{
	uint8_t i, j;
	
	SLEEP = 0;					//SLEEP.MODE = 0 to use HFXO
	while (!(SLEEP & 0x20));	//wait for HFRC to stabilize
	CLKCON = 0x79;				//high speed RC osc, timer clock is 203.125KHz, 13MHz system clock
	while (!(SLEEP & 0x40));	//wait for HFXO to stabilize
	
	//we need to delay more (chip erratum)
	for (i = 0; i != 128; i++) for (j = 0; j != 128; j++) __asm__ ("nop");
	
	CLKCON = 0x39;				//switch to HFXO
	while (CLKCON & 0x40);		//wait for the switch
	CLKCON = 0x38;				//go to 26MHz system and timer speed,  timer clock is Fosc / 128 = 203.125KHz
	SLEEP = 4;					//power down the unused (HFRC oscillator)
}

#pragma save
#pragma nogcse
static void uiPrvDrawImageAtAddress(uint32_t addr)
{
	uint8_t __xdata clutOurs[256], packetBitSz, packetNumPixels, packetPixelDivVal;
	uint16_t __pdata effectiveH, effectiveW, stride, packetsPerRow;
	uint32_t __xdata w, h, clutAddr, imgDataStartAddr;
	uint8_t __xdata rowBuf[SCREEN_WIDTH];				//xxx: we can read in smaller chunks if needed, later
	struct BitmapFileHeader __xdata bmph;
	struct BitmapClutEntry __xdata clut;
	uint16_t __pdata c, r, i;
	uint16_t __xdata clutSz;
	uint8_t bpp, iteration;
	__bit bottomUp;
	
	
	addr += sizeof(struct EepromImageHeader);
	
	eepromRead(addr + 0, &bmph, sizeof(bmph));
	
	if (bmph.sig[0] != 'B' || bmph.sig[1] != 'M')
		return;
	
	if (bmph.colorplanes != 1)
		return;
	
	if (bmph.headerSz < 40)
		return;
	
	if (bmph.bpp > 8)
		return;
	
	if (!(uint32_t)bmph.height)	//without the cast SDCC generates worse code...
		return;
	
	if (bmph.width <= 0)
		return;
	
	if (bmph.numColors > 256)
		return;
	
	if (bmph.height >= 0) {
		h = bmph.height;
		bottomUp = true;
	}
	else {
		h = -bmph.height;
		bottomUp = false;
	}
	
	w = bmph.width;
	bpp = bmph.bpp;
	clutSz = bmph.numColors;
	if (!clutSz)
		clutSz = 1 << bpp;
	
	if (bmph.compression == COMPRESSION_BITPACKED_3x5_to_7 || bmph.compression == COMPRESSION_BITPACKED_5x3_to_8) {
		
		packetNumPixels = (uint8_t)(bmph.compression >> 8) & 0x0f;
		packetBitSz = (uint8_t)bmph.compression & 0x0f;
		packetPixelDivVal = ((uint8_t)bmph.compression) >> 4;
	}
	else if (bmph.compression) {
		
		pr("unknown compression 0x%*08lx\n", (uint16_t)&bmph.compression);
		return;
	}
	else {	//uncompressed
		
		packetPixelDivVal = 0;
		packetNumPixels = 1;
		packetBitSz = bpp;
	}
	
	packetsPerRow = mathPrvDiv16x8(w + packetNumPixels - 1, packetNumPixels);
	stride = (mathPrvMul16x8(packetsPerRow, packetBitSz) + 31) / 32 * 4UL;
	
	//clut does not always follow header(ask GIMP why), but it does precede data
	clutAddr = addr + bmph.dataOfst - sizeof(struct BitmapClutEntry) * clutSz;
	
	//convert clut to our understanding of color
	for (i = 0; i < clutSz; i++, clutAddr += sizeof(struct BitmapClutEntry)) {
		
		uint8_t entry;
		
		eepromRead(clutAddr, &clut, sizeof(clut));
		
		if (SCREEN_EXTRA_COLOR_INDEX >=0 && clut.r == 0xff && (clut.g == 0xff || clut.g == 0) && clut.b == 0)	//yellow/red
			entry = SCREEN_EXTRA_COLOR_INDEX;
		else {
			
			uint16_t intensity = 0;
			
			intensity += mathPrvMul8x8(0x37, clut.r);
			intensity += mathPrvMul8x8(0xB7, clut.g);
			intensity += mathPrvMul8x8(0x12, clut.b);
			//adds up to 0xff00 -> fix it
			intensity += (uint8_t)(intensity >> 8);
			
			entry = mathPrvMul16x8(intensity, SCREEN_NUM_GREYS) >> 16;
			entry += SCREEN_FIRST_GREY_IDX;
		}
		//pr("mapped clut %u (%d %d %d) -> %d\n", i, clut.r, clut.g, clut.b, entry);
		clutOurs[i] = entry;
	}
	
	//replicate clut to avoid having to mask input later
	for (;i < 256; i++)
		clutOurs[i] = clutOurs[i & ((1 << bpp) - 1)];
	
	effectiveH = (h > SCREEN_HEIGHT) ? SCREEN_HEIGHT : h;
	effectiveW = (w > SCREEN_WIDTH) ? SCREEN_WIDTH : w;
	addr += bmph.dataOfst;
	imgDataStartAddr = addr;
	
	screenTxStart(false);
	
	for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++) {

		addr = imgDataStartAddr;
		
		for (r = 0; r < effectiveH; r++) {
			
			uint16_t __pdata effectiveRow = bottomUp ? effectiveH - 1 - r : r, bytesOut = 0;
			uint8_t inIdx = 0, bitpoolInUsed = 0, bitpoolIn = 0;
			
			#if SCREEN_TX_BPP == 4
				uint8_t txPrev = 0;
				__bit emit = false;
			#else
				uint8_t bitpoolOutUsedUsed = 0;
				uint16_t bitpoolOut = 0;
			#endif
			
			//get a row
			eepromRead(addr + mathPrvMul16x16(effectiveRow, stride), rowBuf, stride);
			
			//convert to our format
			c = effectiveW;
			do {
				uint8_t packet, packetIdx, packetMembers = packetNumPixels;
				
				if (bitpoolInUsed >= packetBitSz) {
					bitpoolInUsed -= packetBitSz;
					packet = bitpoolIn >> bitpoolInUsed;
				}
				else {
					
					uint8_t t = rowBuf[inIdx++];
					
					packet = (bitpoolIn << (packetBitSz - bitpoolInUsed)) | (t >> (8 - (packetBitSz - bitpoolInUsed)));
					bitpoolInUsed += 8 - packetBitSz;
					
					bitpoolIn = t;
				}
				packet &= (1 << packetBitSz) - 1;
				
				//val is now a packet - unpack it
				if (packetMembers > c)
					packetMembers = c;
				
				for (packetIdx = 0; packetIdx < packetMembers; packetIdx++) {
					
					uint8_t val;
					
					//extract
					if (packetPixelDivVal) {
						
						val = packet % packetPixelDivVal;
						packet /= packetPixelDivVal;
					}
					else
						val = packet;

					//map
					val = clutOurs[val];
					
					//get bits out
					#if SCREEN_TX_BPP == 4
						
						if (emit) {
							emit = false;
							screenByteTx(txPrev | val);
							bytesOut++;
							txPrev = 0;
						}
						else {
							emit = true;
							txPrev = val << 4;
						}
						
					#else
						bitpoolOut <<= SCREEN_TX_BPP;
						bitpoolOut |= val;
						bitpoolOutUsedUsed += SCREEN_TX_BPP;
						if (bitpoolOutUsedUsed >= 8) {
							screenByteTx(bitpoolOut >> (bitpoolOutUsedUsed -= 8));
							bitpoolOut &= (1 << bitpoolOutUsedUsed) - 1;
							bytesOut++;
						}
					#endif
				}
				c -= packetMembers;
			} while (c);
			#if SCREEN_TX_BPP != 4
				
				if (bitpoolOutUsedUsed) {
					screenByteTx(bitpoolOut);
					bytesOut++;
				}
			#endif
			
			//if we did not produce enough bytes, do so
			bytesOut = ((long)SCREEN_WIDTH * SCREEN_TX_BPP + 7) / 8 - bytesOut;
			while(bytesOut--)
				screenByteTx(SCREEN_BYTE_FILL);
		}
		
		//fill the rest of the screen
		for (;r < SCREEN_HEIGHT; r++) {
			c = ((long)SCREEN_WIDTH * SCREEN_TX_BPP + 7) / 8;
			
			do {
				screenByteTx(SCREEN_BYTE_FILL);
			} while (--c);
		}
		
		screenEndPass();
	}
	
	screenTxEnd();
	screenShutdown();
}
#pragma restore

static void uiPrvFullscreenMsg(const char __xdata *str1, const char __xdata *str2, const char __xdata *str3)
{
	struct CharDrawingParams __xdata cdp;
	uint16_t i, r, textRow, textRowEnd;
	static const __code char zero = 0;
	uint8_t rowIdx, iteration;
	
	#if defined(CHROMA74)
		#define MAGNIFY1		3
		#define MAGNIFY2		2
		#define MAGNIFY3		1
		#define BACK_COLOR		6
		#define FORE_COLOR_1	0
		#define FORE_COLOR_2	2
		#define FORE_COLOR_3	0
		#define MAIN_TEXT_V_CENTERED
	#elif defined(CHROMA29)
		#define MAGNIFY1		1
		#define MAGNIFY2		1
		#define MAGNIFY3		1
		#define BACK_COLOR		3
		#define FORE_COLOR_1	0
		#define FORE_COLOR_2	1
		#define FORE_COLOR_3	0
	#elif defined(EPOP900)
		#define MAGNIFY1		2
		#define MAGNIFY2		2
		#define MAGNIFY3		1
		#define BACK_COLOR		1
		#define FORE_COLOR_1	0
		#define FORE_COLOR_2	0
		#define FORE_COLOR_3	0
		#define MAIN_TEXT_V_CENTERED
	#elif defined(EPOP50)
		#define MAGNIFY1		1
		#define MAGNIFY2		1
		#define MAGNIFY3		1
		#define BACK_COLOR		1
		#define FORE_COLOR_1	0
		#define FORE_COLOR_2	0
		#define FORE_COLOR_3	0
	#else
	#error "screen type not known"
	#endif
	
	if (!str1)
		str1 = (const __xdata char*)&zero;
	if (!str2)
		str2 = (const __xdata char*)&zero;
	if (!str3)
		str3 = (const __xdata char*)&zero;
	
	pr("MESSAGE '%ls' '%ls' '%ls'\n", (uint16_t)str1, (uint16_t)str2, (uint16_t)str3);
	
#if 1
	
	screenTxStart(false);
	
	for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++) {
		
		rowIdx = 0;
		
		cdp.magnify = MAGNIFY1;
		cdp.str = str1;
		cdp.x = mathPrvI16Asr1(SCREEN_WIDTH - mathPrvMul8x8(CHAR_WIDTH * cdp.magnify, xStrLen(cdp.str)));
			
		cdp.foreColor = FORE_COLOR_1;
		cdp.backColor = BACK_COLOR;
		
		#ifdef MAIN_TEXT_V_CENTERED
			textRow = SCREEN_HEIGHT - CHAR_HEIGHT * cdp.magnify;
			textRow >>= 1;
		#else
			textRow = 10;
		#endif
		
		textRowEnd = textRow + CHAR_HEIGHT * cdp.magnify;
		
		for (r = 0; r < SCREEN_HEIGHT; r++) {
			
			//clear the row
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				mScreenRow[i] = SCREEN_BYTE_FILL;
		
			if (r >= textRowEnd) {
				
				switch (rowIdx) {
					case 0:
						rowIdx = 1;
						
						textRow = textRowEnd + 20;
						cdp.magnify = MAGNIFY2;
						cdp.foreColor = FORE_COLOR_2;
						cdp.str = str2;
						cdp.x = mathPrvI16Asr1(SCREEN_WIDTH - mathPrvMul8x8(CHAR_WIDTH * cdp.magnify, xStrLen(cdp.str)));
						textRowEnd = textRow + CHAR_HEIGHT * cdp.magnify;
						break;
					
					case 1:
						rowIdx = 2;
						
						textRow = SCREEN_HEIGHT - CHAR_HEIGHT;
						cdp.magnify = MAGNIFY3;
						cdp.foreColor = FORE_COLOR_3;
						cdp.str = str3;
						cdp.x = 1;
						textRowEnd = textRow + CHAR_HEIGHT;
						break;
					
					case 2:
						cdp.str = (const __xdata char*)&zero;
						break;
				}
			}
			else if (r > textRow) {
				cdp.imgRow = r - textRow;
				charsDrawString(&cdp);
			}
			
			for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
				screenByteTx(mScreenRow[i]);
		}
		
		screenEndPass();
	}
	
	screenTxEnd();
#endif
}

static __bit showVersionAndVerifyMatch(void)
{
	static const __code uint64_t verMask = ~VERSION_SIGNIFICANT_MASK;
	uint8_t i;
	
	pr("Booting FW ver 0x%*016llx\n", (uint16_t)&mVersion);
	
	for (i = 0; i < 8; i++) {
		
		if (((const uint8_t __code*)&mVersion)[i] & ((const uint8_t __code*)&verMask)[i]) {
			pr("ver num @ red zone\n");
			return false;
		}
	}
	
	pr(" -> %ls\n", (uint16_t)fwVerString());
	return true;
}

#pragma callee_saves rndGen
static uint8_t rndGen(void)
{
	ADCCON1 |= 4;
	while (ADCCON1 & 0x0c);
	return RNDH ^ RNDL;
}

#pragma callee_saves rndGen32
static uint32_t rndGen32(void) __naked
{
	__asm__ (
		//there simply is no way to get SDCC to generate this anywhere near as cleanly
		"	lcall  _rndGen		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen		\n"
		"	pop    DPH			\n"
		"	pop    B			\n"
		"	pop    A			\n"
		"	ret					\n" 
	);
}

static void prvFillTagState(struct TagState __xdata *state)
{
	#if defined(CHROMA74)
		#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
			state->hwType = HW_TYPE_74_INCH_DISPDATA_FRAME_MODE;
		#else
			state->hwType = HW_TYPE_74_INCH_DISPDATA;
		#endif
	#elif defined(CHROMA29)
		#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
			state->hwType = HW_TYPE_29_INCH_DISPDATA_FRAME_MODE;
		#else
			state->hwType = HW_TYPE_29_INCH_DISPDATA;
		#endif
	#elif defined(EPOP50)
		#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
			#error "no picture frame more on the quick-dieing epop screen"
		#else
			state->hwType = HW_TYPE_ZBD_EPOP50;
		#endif
	#elif defined (EPOP900)
		#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
			#error "no picture frame more on the quick-dieing epop screen"
		#else
			state->hwType = HW_TYPE_ZBD_EPOP900;
		#endif
	#else
		#error "type not known"
	#endif
	

	u64_copyFromCode(&state->swVer, &mVersion);
	state->batteryMv = adcSampleBattery();
}

static uint32_t uiNotPaired(void)
{
	struct {
		uint8_t pktType;
		struct TagInfo ti;
	} __xdata packet = {0, };
	uint_fast8_t i, ch;
	
	packet.pktType = PKT_ASSOC_REQ;
	packet.ti.protoVer = PROTO_VER_CURRENT;
	prvFillTagState(&packet.ti.state);
	packet.ti.screenPixWidth = SCREEN_WIDTH;
	packet.ti.screenPixHeight = SCREEN_HEIGHT;
	packet.ti.screenMmWidth = SCREEN_WIDTH_MM;
	packet.ti.screenMmHeight = SCREEN_HEIGHT_MM;
	packet.ti.compressionsSupported = PROTO_COMPR_TYPE_BITPACK;
	packet.ti.maxWaitMsec = COMMS_MAX_RADIO_WAIT_MSEC;
	packet.ti.screenType = SCREEN_TYPE;
	
	uiPrvFullscreenMsg((const __xdata char*)"ASSOCIATE READY", macString() + (SCREEN_WIDTH < 192 ? 9 : 0), fwVerString());
	
	timerDelay(TIMER_TICKS_PER_SECOND);
	
	//RX on
	radioRxEnable(true, true);
	for (ch = 0; ch < RADIO_NUM_CHANNELS; ch++) {
		
		pr("try ch %u\n", RADIO_FIRST_CHANNEL + ch);
		radioSetChannel(RADIO_FIRST_CHANNEL + ch);
		
		for (i = 0; i < 4; i++) {							//try 4 times
			
			commsTx(&mCi, true, &packet, sizeof(packet));
			
			mTimerWaitStart = timerGet();
			while (timerGet() - mTimerWaitStart < TIMER_TICKS_PER_SECOND / 4) {// wait 250 ms before retransmitting
				
				int8_t ret;
				
				ret = commsRx(&mCi, mRxBuf, mSettings.masterMac);
				
				if (ret == COMMS_RX_ERR_NO_PACKETS)
					continue;
				
				pr("RX pkt: 0x%02x + %d\n", mRxBuf[0], ret);
				
				if (ret == COMMS_RX_ERR_MIC_FAIL)
					pr("RX: invalid MIC\n");
				else if (ret <= 0)
					pr("WTF else ret = %x\n", (int16_t)(int8_t)ret);	//nothing
				else if (ret < sizeof(uint8_t) + sizeof(struct AssocInfo))
					pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));
				else if (mRxBuf[0] != PKT_ASSOC_RESP)
					pr("RX: pkt 0x%02x @ pair\n", mRxBuf[0]);
				else if (commsGetLastPacketRSSI() < -60)
                    pr("RX: too weak to associate: %d\n", commsGetLastPacketRSSI());
				else {
					struct AssocInfo __xdata *ai = (struct AssocInfo __xdata*)(mRxBuf + 1);
					
					mSettings.checkinDelay = ai->checkinDelay;
					mSettings.retryDelay = ai->retryDelay;
					mSettings.failedCheckinsTillBlank = ai->failedCheckinsTillBlank;
					mSettings.failedCheckinsTillDissoc = ai->failedCheckinsTillDissoc;
					mSettings.channel = ch;
					mSettings.numFailedCheckins = 0;
					mSettings.nextIV = 0;
					xMemCopy(mSettings.encrKey, ai->newKey, sizeof(mSettings.encrKey));
					mSettings.isPaired = 1;
					
					
					//power the radio down
					radioRxEnable(false, false);
					
					pr("Associated to master %m\n", (uint16_t)&mSettings.masterMac);
					
					pr("Erz IMG\n");
					eepromErase(EEPROM_IMG_START, mathPrvMul16x8(EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ, mNumImgSlots));
					
					pr("Erz UPD\n");
					eepromErase(EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ);
					
					uiPrvFullscreenMsg((const __xdata char*)"\x01\x02", macString() + (SCREEN_WIDTH < 192 ? 9 : 0), fwVerString());	//signal icon
					
					return 1000;	//wake up in a second to check in
				}
			}
		}
	}
	
	//power the radio down
	radioRxEnable(false, false);
	
	uiPrvFullscreenMsg((const __xdata char*)"\x03\x04", macString() + (SCREEN_WIDTH < 192 ? 9 : 0), fwVerString());	//no signal icon
	
	return 0;	//never
}

static struct PendingInfo __xdata* prvSendCheckin(void)
{
	struct {
		uint8_t pktTyp;
		struct CheckinInfo cii;
	} __xdata packet = {.pktTyp = PKT_CHECKIN,};
	uint8_t __xdata fromMac[8];
	
	prvFillTagState(&packet.cii.state);
	
	packet.cii.lastPacketLQI = mSettings.lastRxedLQI;
	packet.cii.lastPacketRSSI = mSettings.lastRxedRSSI;
	packet.cii.temperature = mCurTemperature + CHECKIN_TEMP_OFFSET;
	
	if (!commsTx(&mCi, false, &packet, sizeof(packet))) {
		pr("Fail to TX checkin\n");
		return 0;
	}
	
	mTimerWaitStart = timerGet();
	while (timerGet() - mTimerWaitStart < (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND * COMMS_MAX_RADIO_WAIT_MSEC / 1000)) {
		
		int8_t ret = commsRx(&mCi, mRxBuf, fromMac);
		
		if (ret == COMMS_RX_ERR_NO_PACKETS)
			continue;
			
		pr("RX pkt: 0x%02x + %d\n", mRxBuf[0], ret);
		
		if (ret == COMMS_RX_ERR_MIC_FAIL) {
			
			pr("RX: invalid MIC\n");
			return 0;
		}
		
		if (ret < sizeof(uint8_t) + sizeof(struct PendingInfo)) {
			
			pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct PendingInfo));
			return 0;
		}
		
		if (mRxBuf[0] != PKT_CHECKOUT) {
			pr("RX: pkt 0x%02x @ checkin\n", mRxBuf[0]);
			return 0;
		}
		
		return (struct PendingInfo __xdata*)(mRxBuf + 1);
	}
	
	return 0;
}

static void uiPrvDrawImageAtSlotIndex(uint8_t idx)
{
	mSettings.lastShownImgSlotIdx = idx;
	uiPrvDrawImageAtAddress(EEPROM_IMG_START + (mathPrvMul16x8(EEPROM_IMG_EACH >> 8, idx) << 8));
}

//copied to ram, after update has been verified, interrupts have been disabled, and eepromReadStart() has been called
//does not return (resets using WDT)
//this func wraps the update code and returns its address (in DPTR), len in B
static uint32_t prvUpdateApplierGet(void) __naked
{
	__asm__(
		"	mov   DPTR, #00098$			\n"
		"	mov   A, #00099$			\n"
		"	clr   C						\n"
		"	subb  A, DPL				\n"
		"	mov   B, A					\n"
		"	ret							\n"
		
		///actual updater code
		"00098$:						\n"
	
		"	mov   B, #32				\n"
		//erase all flash
		"	clr   _FADDRH				\n"
		"	clr   _FADDRL				\n"	
		"00001$:						\n"
		"	orl   _FCTL, #0x01			\n"
		"	nop							\n"	//as per datasheet
		"00002$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.7, 00002$			\n"
		"	inc   _FADDRH				\n"
		"	inc   _FADDRH				\n"
		"	djnz  B, 00001$				\n"
		
		//write all 32K
		//due to the 40 usec timeout, we wait each time to avoid it
		"	mov   DPTR, #0			\n"
		
		"00003$:						\n"
		"	mov   _FADDRH, DPH			\n"
		"	mov   _FADDRL, DPL			\n"
		"	inc   DPTR					\n"
		"	mov   _FWT, #0x22			\n"
		
		//get two bytes
	
		"   mov   B, #2					\n"
		
		"00090$:						\n"
		"	mov   _U1DBUF, #0x00		\n"
		
		"00091$:						\n"
		"	mov   A, _U1CSR				\n"
		"	jnb   A.1, 00091$			\n"
		
		"	anl   _U1CSR, #~0x02		\n"
		
		"00092$:						\n"
		"	mov   A, _U1CSR				\n"
		"	jb    A.0, 00092$			\n"
		
		"	push  _U1DBUF				\n"
		"	djnz  B, 00090$				\n"
		
		//write two bytes
		"	orl   _FCTL, #0x02			\n"
		
		//wait for fwbusy to go low
		"00012$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.6, 00012$			\n"
		
		"	pop   A						\n"
		"	pop   _FWDATA				\n"
		"	mov   _FWDATA, A			\n"
		
		//wait for swbusy to be low
		"00004$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.6, 00004$			\n"
		
		"	anl   _FCTL, #~0x02			\n"
		
		//wait for busy to be low
		"00005$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.7, 00005$			\n"
		
		//loop for next two bytes
		"	mov   A, DPH				\n"
		"	cjne  A, #0x40, 00003$		\n"
		
		//done
	
		//WDT reset
		"	mov   _WDCTL, #0x0b			\n"
		"00007$:						\n"
		"	sjmp  00007$				\n"
		
		"00099$:						\n"
	);
}

static void prvApplyUpdateIfNeeded(void)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata*)mScreenRow;
	uint32_t updaterInfo = prvUpdateApplierGet();
	void __code *src = (void __code*)updaterInfo;
	uint8_t len = updaterInfo >> 16;
	
	eepromRead(EEPROM_UPDATA_AREA_START, eih, sizeof(struct EepromImageHeader));
	
	if (eih->validMarker == EEPROM_IMG_INPROGRESS)
		pr("update: not fully downloaded\n");
	else if (eih->validMarker == 0xffffffff)
		pr("update: nonexistent\n");
	else if (eih->validMarker != EEPROM_IMG_VALID)
		pr("update: marker invalid: 0x%*08lx\n", (uint16_t)&eih->validMarker);
	else {
		
		uint64_t __xdata tmp64 = VERSION_SIGNIFICANT_MASK;
		u64_and(&tmp64, &eih->version);
		
		if (!u64_isLt((const uint64_t __xdata *)&mVersion, &tmp64)) {
			
			pr("update is not new enough: 0x%*016llx, us is: 0x%*016llx. Erasing\n", (uint16_t)&eih->version, (uint16_t)&mVersion);
			
			eepromErase(EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ);
		}
		else {
		
			pr("applying the update 0x%*016llx -> 0x%*016llx\n", (uint16_t)&mVersion, (uint16_t)&eih->version);

			xMemCopy(mScreenRow, (void __xdata*)src, len);
			
			DMAARM = 0xff;	//all DMA channels off
			IEN0 = 0;	//ints off
			
			eepromReadStart(EEPROM_UPDATA_AREA_START + sizeof(struct EepromImageHeader));
			MEMCTR = 3;	//cache and prefetch off
			
			__asm__(
				"	mov dptr, #_mScreenRow		\n"
				"	clr a						\n"
				"	jmp @a+dptr					\n"
			);
		}
		
	}
}

//prevStateP should be 0xffff for first invocation
static void prvProgressBar(uint16_t done, uint16_t outOf, uint16_t __xdata *prevStateP)
{
	#if defined(SCREEN_PARTIAL_W2B) && defined(SCREEN_PARTIAL_B2W) && defined(SCREEN_PARTIAL_KEEP)
	
		uint16_t now = mathPrvDiv32x16(mathPrvMul16x16(done, SCREEN_WIDTH * SCREEN_TX_BPP / 8) + (outOf / 2), outOf);		//in bytes
		__bit blacken, redraw = false;
		uint16_t i, j, min, max;
		uint8_t iteration;
		
		if (*prevStateP == 0xffff) {
			redraw = true;
		}
		if (now < *prevStateP) {
			min = now;
			max = *prevStateP;
			blacken = false;
		}
		else if (now == *prevStateP) {
			
			return;
		}
		else {
			blacken = true;
			min = *prevStateP;
			max = now;
		}
		*prevStateP = now;
		
		if (!screenTxStart(true))
			return;
		
		for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++) {
	
			if (redraw) {
				for (i = 0; i < now; i++)
					screenByteTx(SCREEN_PARTIAL_W2B);
				for (; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					screenByteTx(SCREEN_PARTIAL_B2W);
				for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					screenByteTx(SCREEN_PARTIAL_B2W);
			}
			else {
				for (i = 0; i < min; i++)
					screenByteTx(SCREEN_PARTIAL_KEEP);
				for (; i < max; i++)
					screenByteTx(blacken ? SCREEN_PARTIAL_W2B : SCREEN_PARTIAL_B2W);
				for (; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					screenByteTx(SCREEN_PARTIAL_KEEP);
				for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					screenByteTx(SCREEN_PARTIAL_KEEP);
			}
			
			//fill rest
			for (j = 0; j < SCREEN_HEIGHT - 2; j++){
				for (i = 0; i < SCREEN_WIDTH * SCREEN_TX_BPP / 8; i++)
					screenByteTx(SCREEN_PARTIAL_KEEP);
			}
			
			screenEndPass();
		}
		screenTxEnd();
		
	#elif defined(SCREEN_PARTIAL_W2B) || defined(SCREEN_PARTIAL_B2W) || defined(SCREEN_PARTIAL_KEEP)
		#error "some but not all partial defines found - this is an error"
	#endif
}

static uint32_t prvDriveDownload(struct EepromImageHeader __xdata *eih, uint32_t addr, __bit isOS)
{
	struct {
		uint8_t pktTyp;
		struct ChunkReqInfo cri;
	} __xdata packet = {.pktTyp = PKT_CHUNK_REQ, .cri = { .osUpdatePlz = isOS, }, };
	uint16_t nPieces = mathPrvDiv32x8(eih->size + EEPROM_PIECE_SZ - 1, EEPROM_PIECE_SZ);
	struct ChunkInfo __xdata *chunk = (struct ChunkInfo*)(mRxBuf + 1);
	uint8_t __xdata *data = (uint8_t*)(chunk + 1);
	__bit progressMade = false;
	uint16_t curPiece;
	
	//sanity check
	if (nPieces > sizeof(eih->piecesMissing) * 8) {
		pr("DL too large: %*lu\n", (uint16_t)&eih->size);
		return mSettings.checkinDelay;
	}
	
	//prepare the packet
	u64_copy(&packet.cri.versionRequested, &eih->version);
	
	//find where we are in downloading
	for (curPiece = 0; curPiece < nPieces && !((eih->piecesMissing[curPiece / 8] >> (curPiece % 8)) & 1); curPiece++);
	
	pr("Requesting piece %u/%u of %ls\n", curPiece, nPieces, isOS ? (uint16_t)"UPDATE" : (uint16_t)"IMAGE");
	
	//download
	for (;curPiece < nPieces; curPiece++) {
		
		uint8_t now, nRetries;
		int8_t ret;
		
		//any piece that is not last will be of standard size
		if (curPiece != nPieces - 1)
			now = EEPROM_PIECE_SZ;
		else
			now = eih->size - mathPrvMul16x8(nPieces - 1, EEPROM_PIECE_SZ);
		
		packet.cri.offset = mathPrvMul16x8(curPiece, EEPROM_PIECE_SZ);
		packet.cri.len = now;
		
		if (!(((uint8_t)curPiece) & 0x7f))
			prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);
		
		for (nRetries = 0; nRetries < 5; nRetries++) {
			
			commsTx(&mCi, false, &packet, sizeof(packet));
			
			mTimerWaitStart = timerGet();
			while (1) {
				
				if (timerGet() - mTimerWaitStart > (uint32_t)((uint64_t)TIMER_TICKS_PER_SECOND * COMMS_MAX_RADIO_WAIT_MSEC / 1000)) {
					pr("RX timeout in download\n");
					break;
				}
				
				ret = commsRx(&mCi, mRxBuf, mSettings.masterMac);
				
				if (ret == COMMS_RX_ERR_NO_PACKETS)
					continue;	//let it time out
				else if (ret == COMMS_RX_ERR_INVALID_PACKET)
					continue;	//let it time out
				else if (ret == COMMS_RX_ERR_MIC_FAIL) {
					pr("RX: invalid MIC\n");
					
					//mic errors are unlikely unless someone is deliberately messing with us - check in later
					goto checkin_again;
				}
				else if ((uint8_t)ret < (uint8_t)(sizeof(uint8_t) + sizeof(struct ChunkInfo))) {
					pr("RX: %d < %d\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));
					
					//server glitch? check in later
					return mSettings.checkinDelay;
				}
				else if (mRxBuf[0] != PKT_CHUNK_RESP) {
					pr("RX: pkt 0x%02x @ DL\n", mRxBuf[0]);
					
					//weird packet? worth retrying soner
					break;
				}
				
				//get payload len
				ret -= sizeof(uint8_t) + sizeof(struct ChunkInfo);
				
				if (chunk->osUpdatePlz != isOS) {
					pr("RX: wrong data type @ DL: %d\n", chunk->osUpdatePlz);
					continue;	//could be an accidental RX of older packet - ignore
				}
				else if (chunk->offset != packet.cri.offset) {
					pr("RX: wrong ofst @ DL 0x%08lx != 0x%*08lx\n", chunk->offset, (uint16_t)&packet.cri.offset);
					continue;	//could be an accidental RX of older packet - ignore
				}
				else if (!ret) {
					pr("RX: DL not avail\n");
					
					//just check in later
					goto checkin_again;
				}
				else if ((uint8_t)ret != (uint8_t)packet.cri.len) {
					
					pr("RX: Got %ub, reqd %u\n", ret, packet.cri.len);
					
					//server glitch? check in later
					goto checkin_again;
				}
				
				//write data
				eepromWrite(addr + mathPrvMul16x8(curPiece, EEPROM_PIECE_SZ) + sizeof(struct EepromImageHeader), data, ret);
				
				//write marker
				eih->piecesMissing[curPiece / 8] &=~ (1 << (curPiece % 8));
				eepromWrite(addr + offsetof(struct EepromImageHeader, piecesMissing[curPiece / 8]), &eih->piecesMissing[curPiece / 8], 1);
				
				progressMade = true;
				nRetries = 100;	//so we break the loop
				break;
			}
		}
		if (nRetries == 5) {
			pr("retried too much\n");
			if (progressMade)
				goto retry_later;
			else
				goto checkin_again;
		}
	}
	
downloadDone:
	pr("Done at piece %u/%u\n", curPiece, nPieces);
	prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);

	//if we are here, we succeeeded in finishing the download	
	eih->validMarker = EEPROM_IMG_VALID;
	eepromWrite(addr + offsetof(struct EepromImageHeader, validMarker), &eih->validMarker, sizeof(eih->validMarker));

	pr("DL completed\n");
	mSettings.prevDlProgress = 0xffff;
	
	//power the radio down
	radioRxEnable(false, false);
	
	//act on it
	if (isOS)
		prvApplyUpdateIfNeeded();
	else if (eih->size >= sizeof(struct BitmapFileHeader)) {
		mSettings.lastShownImgSlotIdx = 0xffff;
		mSettings.checkinsToFlipCtr = 0;
		uiPrvDrawImageAtAddress(addr);
		pr("image drawn\n");
	}
	
checkin_again:
	if (curPiece != nPieces)
		prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);
	return mSettings.checkinDelay;

retry_later:
	prvProgressBar(curPiece, nPieces, &mSettings.prevDlProgress);
	return mSettings.retryDelay;
}

static void prvWriteNewHeader(struct EepromImageHeader __xdata *eih, uint32_t addr, uint8_t eeNumSecs, uint64_t __xdata *verP, uint32_t size)
{
	//zero it
	xMemSet(eih, 0, sizeof(struct EepromImageHeader));
	
	//mark all pieces missing
	xMemSet(eih->piecesMissing, 0xff, sizeof(eih->piecesMissing));
	
	eepromErase(addr, eeNumSecs);
	
	u64_copy(&eih->version, verP);
	eih->validMarker = EEPROM_IMG_INPROGRESS;
	eih->size = size;
	
	eepromWrite(addr, eih, sizeof(struct EepromImageHeader));
}

static uint32_t prvDriveUpdateDownload(uint64_t __xdata *verP, uint32_t size)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata*)mScreenRow;		//use screen buffer
	
	//see what's there already
	eepromRead(EEPROM_UPDATA_AREA_START, eih, sizeof(struct EepromImageHeader));
	if (!u64_isEq(&eih->version, verP))
		prvWriteNewHeader(eih, EEPROM_UPDATA_AREA_START, EEPROM_UPDATE_AREA_LEN / EEPROM_ERZ_SECTOR_SZ, verP, size);
	
	return prvDriveDownload(eih, EEPROM_UPDATA_AREA_START, true);
}

static uint32_t prvDriveImageDownload(const struct EepromContentsInfo __xdata *eci, uint64_t __xdata *verP, uint32_t size)
{
	struct EepromImageHeader __xdata *eih = (struct EepromImageHeader __xdata*)mScreenRow;		//use screen buffer
	uint32_t __pdata addr;
	
	//sort out where next image should live
	if (eci->latestInprogressImgAddr)
		addr = eci->latestInprogressImgAddr;
	else if (!eci->latestCompleteImgAddr)
		addr = EEPROM_IMG_START;
	else {
		addr = eci->latestCompleteImgAddr + EEPROM_IMG_EACH;
		
		if (addr >= EEPROM_IMG_START + (mathPrvMul16x8(EEPROM_IMG_EACH >> 8, mNumImgSlots) << 8))
			addr = EEPROM_IMG_START;
	}
	
	//see what's there already
	eepromRead(addr, eih, sizeof(struct EepromImageHeader));
	if (!u64_isEq(&eih->version, verP))
		prvWriteNewHeader(eih, addr, EEPROM_IMG_EACH / EEPROM_ERZ_SECTOR_SZ, verP, size);
	
	return prvDriveDownload(eih, addr, false);
}

static __bit uiPrvIsShowingTooLongWithNoCheckinsMessage(void)
{
	return mSettings.failedCheckinsTillBlank && mSettings.numFailedCheckins >= mSettings.failedCheckinsTillBlank;
}

#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
	static void uiPrvPictureFrameFlip(struct EepromContentsInfo __xdata *eci)
	{	
		//if we're showing the "too long since checkin" message, do nothing
		if (uiPrvIsShowingTooLongWithNoCheckinsMessage())
			return;
	
		if (++mSettings.checkinsToFlipCtr >= PICTURE_FRAME_FLIP_EVERY_N_CHECKINS) {
			
			uint16_t prev;
			uint8_t n;
			
			mSettings.checkinsToFlipCtr = 0;
			
			if (!eci->numValidImages)
				return;
			else if (eci->numValidImages == 1)
				n = 0;
			else {
				n = mathPrvMod32x16(rndGen32(), eci->numValidImages - 1);
			
				if (n >= mSettings.lastShownImgSlotIdx)
					n++;
			}
			prev = mSettings.lastShownImgSlotIdx;
			uiPrvDrawNthValidImage(n);
			pr("Flip %u->%u\n", prev, mSettings.lastShownImgSlotIdx);
		}
	}
#endif

static uint32_t uiPaired(void)
{
	__bit updateOs, updateImg, checkInSucces = false, wasShowingError;
	uint64_t __xdata tmp64 = VERSION_SIGNIFICANT_MASK;
	struct EepromContentsInfo __xdata eci;
	struct PendingInfo __xdata *pi;
	uint8_t i;
	
	//do this before we get started with the radio
	prvEepromIndex(&eci);
	
	#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
		uiPrvPictureFrameFlip(&eci);
	#endif
	
	//power the radio up
	radioRxEnable(true, true);
	
	//try five times
	for (i = 0; i < 5; i++) {
		pi = prvSendCheckin();
		if (pi) {
			checkInSucces = true;
			break;
		}
	}
	 
	if (!checkInSucces) {	//fail
		
		mSettings.numFailedCheckins++;
		if (!mSettings.numFailedCheckins)	//do not allow overflow
			mSettings.numFailedCheckins--;
		pr("checkin #%u fails\n", mSettings.numFailedCheckins);
		
		//power the radio down
		radioRxEnable(false, false);
		
		if (mSettings.failedCheckinsTillDissoc && mSettings.numFailedCheckins == mSettings.failedCheckinsTillDissoc) {
			pr("Disassoc as %u = %u\n", mSettings.numFailedCheckins, mSettings.failedCheckinsTillDissoc);
			mSettings.isPaired = 0;
			
			return 1000;	//wake up in a second to try to pair
		}
		
		if (mSettings.failedCheckinsTillBlank && mSettings.numFailedCheckins == mSettings.failedCheckinsTillBlank) {
			mSettings.lastShownImgSlotIdx = 0xffff;
			pr("Blank as %u = %u\n", mSettings.numFailedCheckins, mSettings.failedCheckinsTillBlank);
			uiPrvFullscreenMsg((const __xdata char*)"NO SIGNAL", macString() + (SCREEN_WIDTH < 192 ? 9 : 0), fwVerString());
		}
		
		//try again in due time
		return mSettings.checkinDelay;
	}

	//if we got here, we succeeded with the check-in, but store if we were showing the error ...
	wasShowingError = uiPrvIsShowingTooLongWithNoCheckinsMessage();
	mSettings.numFailedCheckins = 0;

	//if screen was blanked, redraw it
	if (wasShowingError) {
		
		#ifdef PICTURE_FRAME_FLIP_EVERY_N_CHECKINS
			uiPrvPictureFrameFlip(&eci);
		#else
			uiPrvDrawImageAtSlotIndex(eci.latestImgIdx);
		#endif
	}
	
	
	//if there is an update, we want it. we know our version number is properly masked as we checked it at boot
	u64_and(&tmp64, &pi->osUpdateVer);
	updateOs = u64_isLt((uint64_t __xdata*)&mVersion, &tmp64);
	updateImg = u64_isLt(&eci.latestCompleteImgVer, &pi->imgUpdateVer);
	
	pr("Base: OS  ver 0x%*016llx, us 0x%*016llx (upd: %u)\n", (uint16_t)&pi->osUpdateVer, (uint16_t)&mVersion, updateOs);
	pr("Base: IMG ver 0x%*016llx, us 0x%*016llx (upd: %u)\n", (uint16_t)&pi->imgUpdateVer, (uint16_t)&eci.latestCompleteImgVer, updateImg);

	if (updateOs)
		return prvDriveUpdateDownload(&pi->osUpdateVer, pi->osUpdateSize);
	
	if (updateImg)
		return prvDriveImageDownload(&eci, &pi->imgUpdateVer, pi->imgUpdateSize);
	
	//nothing? guess we'll check again later
	return mSettings.checkinDelay;
}

static int8_t prvReadSetting(uint8_t type, uint8_t __xdata *dst, uint8_t maxLen)	//returns token length if found, copies at most maxLen. returns -1 if not found
{
	static const uint8_t __xdata magicNum[4] = {0x56, 0x12, 0x09, 0x85};
	uint8_t __xdata tmpBuf[4];
	uint8_t pg, gotLen = 0;
	__bit found = false;
	
	for (pg = 0; pg < 10; pg++) {
		
		uint16_t addr = mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, pg);
		uint16_t ofst = 4;
		
		eepromRead(addr, tmpBuf, 4);
		
		if (xMemEqual(tmpBuf, magicNum, 4)) {
		
			while (ofst < EEPROM_ERZ_SECTOR_SZ) {
		
				eepromRead(addr + ofst, tmpBuf, 2);// first byte is type, (0xff for done), second is length
				if (tmpBuf[0] == 0xff)
					break;
				
				if (tmpBuf[0] == type && tmpBuf[1] >= 2) {
					
					uint8_t copyLen = gotLen = tmpBuf[1] - 2;
					if (copyLen > maxLen)
						copyLen = maxLen;
					
					eepromRead(addr + ofst + 2, dst, copyLen);
					found = true;
				}
				ofst += tmpBuf[1];
			}
		}
	}
	return found ? gotLen : -1;
}

static void powerDownPorts(void)
{
	P0 = 0b01000000;
	P1 = 0b01000000;
	P2 = 0b00000001;
	P0DIR = 0b11111111;
	P1DIR = 0b01101110;
	P2DIR = 0b00000001;
	P0SEL = 0;
	P1SEL = 0;
	P2SEL = 0;
}

void main(void)
{
	volatile /* do not ask, and i will not tell */uint32_t __pdata sleepDuration = 0;
	uint32_t eeSize;
	uint16_t nSlots;
	
	IEN0 = 0;
	IEN1 = 0;
	IEN2 = 0;
	MEMCTR = 0;	//enable prefetch
	
	clockingInit();
	timerInit();
	radioInit();
	u1init();
	
	if (!eepromInit()) {
		pr("failed to init eeprom\n");
		goto out;
	}

	IEN0 |= 1 << 7;	//irqs on
	
	if (!showVersionAndVerifyMatch())
		goto out;
	
	if (prvReadSetting(0x2a, mSelfMac + 1, 7) < 0 && prvReadSetting(1, mSelfMac + 1, 6) < 0) {
		pr("failed to get MAC. Aborting\n");
		while(1);
	}
	
	#ifdef SCREEN_EXPECTS_VCOM
		if (prvReadSetting(0x23, &mScreenVcom, 1) < 0) {
			pr("failed to get VCOM\n");
			mScreenVcom = 0x28;	//a sane value
		}
		pr("VCOM: 0x%02x\n", mScreenVcom);
	#endif
	
	if (prvReadSetting(0x12, &mAdcSlope, 2) < 0) {
		pr("failed to get ADC slope\n");
		mAdcSlope = 2470;	//a sane value
	}
	if (prvReadSetting(0x09, &mAdcIntercept, 2) < 0) {
		pr("failed to get ADC intercept\n");
		mAdcIntercept = 755;	//a sane value
	}
	
	//reformat MAC how we need it
	mSelfMac[0] = mSelfMac[6];
	mSelfMac[1] = mSelfMac[5];
	mSelfMac[2] = mSelfMac[4];
	mSelfMac[4] = (uint8_t)(MAC_SECOND_WORD >> 0);
	mSelfMac[5] = (uint8_t)(MAC_SECOND_WORD >> 8);
	mSelfMac[6] = (uint8_t)(MAC_SECOND_WORD >> 16);
	mSelfMac[7] = (uint8_t)(MAC_SECOND_WORD >> 24);
	pr("MAC %ls\n", (uint16_t)macString());
	pr("ADC: %u %u\n", mAdcSlope, mAdcIntercept);
	
	mCurTemperature = adcSampleTemperature();
	pr("temp: %d\n", mCurTemperature);
	
	//sort out how many images EEPROM stores
	#ifdef EEPROM_IMG_LEN
		
		mNumImgSlots = EEPROM_IMG_LEN / EEPROM_IMG_EACH;
		
	#else
		eeSize = eepromGetSize();
		nSlots = mathPrvDiv32x16(eeSize - EEPROM_IMG_START, EEPROM_IMG_EACH >> 8) >> 8;
		if (eeSize < EEPROM_IMG_START || !nSlots) {
			
			pr("eeprom is too small\n");
			goto out;
		}
		else if (nSlots >> 8) {
			
			pr("eeprom is too big, some will be unused\n");
			mNumImgSlots = 255;
		}
		else
			mNumImgSlots = nSlots;
	#endif
	pr("eeprom has %u image slots\n", mNumImgSlots);
	
	if (0) {
		
		screenTest();
		while(1);
	}
	else if (0) {
		struct EepromContentsInfo __xdata eci;
		prvEepromIndex(&eci);
		uiPrvDrawImageAtSlotIndex(eci.latestImgIdx);
	}
	else {

		settingsRead(&mSettings);
		
		radioRxFilterCfg(mSelfMac, 0x10000, PROTO_PAN_ID);
		
		mCi.myMac = mSelfMac;
		mCi.masterMac = mSettings.masterMac;
		mCi.encrKey = mSettings.encrKey;
		mCi.nextIV = &mSettings.nextIV;
		
		//init the "random" number generation unit
		RNDL = mSelfMac[0] ^ (uint8_t)timerGetLowBits();
		RNDL = mSelfMac[1] ^ (uint8_t)mSettings.hdr.revision;
	
		if (mSettings.isPaired) {
			
			radioSetChannel(RADIO_FIRST_CHANNEL + mSettings.channel);
			radioSetTxPower(10);
			
			prvApplyUpdateIfNeeded();
			
			sleepDuration = uiPaired();
		}
		else {
			
			static const uint32_t __code presharedKey[] = PROTO_PRESHARED_KEY;
			
			radioSetTxPower(-10);	//rather low
			
			xMemCopy(mSettings.encrKey, (const void __xdata*)presharedKey, sizeof(mSettings.encrKey));
	
			sleepDuration = uiNotPaired();
		}
	
		mSettings.lastRxedLQI = commsGetLastPacketLQI();
		mSettings.lastRxedRSSI = commsGetLastPacketRSSI();
		
		settingsWrite(&mSettings);
		
		//vary sleep a little to avoid repeated collisions. Only down because 8-bit math is hard...
		sleepDuration = mathPrvMul32x8(sleepDuration, (rndGen() & 31) + 224) >> 8;
	out:
		pr("sleep: %lu\n", sleepDuration);
		eepromDeepPowerDown();
		screenShutdown();
		
		powerDownPorts();
	
		sleepForMsec(sleepDuration);		//does not return, resets
	}
	while(1);
}
