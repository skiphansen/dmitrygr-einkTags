#include "settings.h"
#include "display.h"
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include "eeprom.h"
#include <stdio.h>
#include "proto.h"
#include "comms.h"
#include "chars.h"
#include "mz100.h"
#include "timer.h"
#include "heap.h"
#include "util.h"
#include "ccm.h"
#include "fw.h"



#define SW_VER_CURRENT				(0x0000010000000007ull)		//top 16 bits are off limits, xxxx.VV.tt.vvvv.mmmm means version V.t.v.m


//unreadable to our code!
uint64_t __attribute__((used, section(".ver"))) mCurVersionExport = SW_VER_CURRENT;

static uint8_t mSelfMac[8];

struct EepromContentsInfo {
	uint32_t latestCompleteImgAddr, latestInprogressImgAddr, latestCompleteImgSize;
	uint64_t latestCompleteImgVer, latestInprogressImgVer;
};

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
	
} __attribute__((packed));

struct BitmapClutEntry {
	uint8_t b, g, r, x;
} __attribute__((packed));

static void prvEepromIndex(struct EepromContentsInfo *eci)
{
	struct EepromImageHeader *eih = (struct EepromImageHeader*)heapAlloc(sizeof(struct EepromImageHeader));
	uint32_t addr;
	
	bzero(eci, sizeof(struct EepromContentsInfo));
	
	for (addr = EEPROM_IMG_START; addr - EEPROM_IMG_START < EEPROM_IMG_LEN; addr += EEPROM_IMG_EACH) {
		
		uint32_t *addrP, *szP = NULL;
		uint64_t *verP = NULL;
		
		(void)qspiRead(0, addr, eih, sizeof(struct EepromImageHeader));
		pr("DATA slot 0x%06x: type 0x%08x ver 0x%08x%08x\r\n", addr, eih->validMarker, (uint32_t)(eih->version >> 32), (uint32_t)eih->version);
		
		switch (eih->validMarker) {
			
			case EEPROM_IMG_INPROGRESS:
				verP = &eci->latestInprogressImgVer;
				addrP = &eci->latestInprogressImgAddr;
				break;
			
			case EEPROM_IMG_VALID:
				verP = &eci->latestCompleteImgVer;
				addrP = &eci->latestCompleteImgAddr;
				szP = &eci->latestCompleteImgSize;
				break;
		}
		
		if (verP && eih->version >= *verP) {
			*verP = eih->version;
			*addrP = addr;
			if (szP)
				*szP = eih->size;
		}
	}
	
	heapFree(eih);
}

static bool prvApplyUpdateIfNeeded(void)	//return true if a reboot is needed
{
	struct EepromImageHeader *eih = (struct EepromImageHeader*)heapAlloc(sizeof(struct EepromImageHeader));
	uint32_t ofst, now, size, pieceSz = 0x2000;
	uint8_t *chunkStore = heapAlloc(pieceSz);
	bool reboot = false;
	
	qspiRead(0, EEPROM_UPDATE_START, eih, sizeof(struct EepromImageHeader));
	
	if (eih->validMarker != EEPROM_IMG_VALID)
		return false;
	
	if (eih->version > SW_VER_CURRENT) {
		pr("Applying update to ver 0x%08x%08x\r\n", (uint32_t)(eih->version >> 32), (uint32_t)eih->version);
		
		pr("Erz 0x%06x .. 0x%06x\r\n", EEPROM_OS_START, EEPROM_OS_START + eih->size - 1);
		qspiEraseRange(EEPROM_OS_START, eih->size);
		
		size = eih->size;
		//from now on, eih (screenbuffer) will be reused for temp storage
		
		for (ofst = 0; ofst < size; ofst += now) {
			
			now = size - ofst;
			if (now > pieceSz)
				now = pieceSz;
			
			pr("Cpy 0x%06x + 0x%04x to 0x%06x\r\n", EEPROM_UPDATE_START + ofst + sizeof(struct EepromImageHeader), now, EEPROM_OS_START + ofst);
			qspiRead(0, EEPROM_UPDATE_START + ofst + sizeof(struct EepromImageHeader), chunkStore, now);
			qspiWrite(false, EEPROM_OS_START + ofst, chunkStore, now);
			
			wdtPet();
		}
		
		reboot = true;
	}
	
	pr("Erz update\r\n");
	qspiEraseRange(EEPROM_UPDATE_START, EEPROM_UPDATE_LEN);

	heapFree(chunkStore);
	heapFree(eih);

	return reboot;
}

static void prvFillTagState(struct Settings *settings, struct TagState *state)
{
	state->hwType = HW_TYPE_42_INCH_SAMSUNG;
	state->swVer = SW_VER_CURRENT;
	state->batteryMv = measureBattery();
}

static bool prvSendCheckin(struct Settings *settings, struct CommsInfo *ci, struct PendingInfo *out)
{
	struct {
		uint8_t pktTyp;
		struct CheckinInfo cii;
	} packet = {};
	uint8_t rx[COMMS_MAX_PACKET_SZ], fromMac[8];
	uint64_t now;
		
	packet.pktTyp = PKT_CHECKIN;
	prvFillTagState(settings, &packet.cii.state);
	
	packet.cii.lastPacketLQI = settings->lastRxedLQI;
	packet.cii.lastPacketRSSI = settings->lastRxedRSSI;
	
	if (!commsTx(ci, false, &packet, sizeof(packet))) {
		pr("Fail to TX checkin\r\n");
		return false;
	}
	
	now = timerGet();
	while (timerGet() - now < TIMER_TICKS_PER_MSEC * COMMS_MAX_RADIO_WAIT_MSEC) {
		
		int32_t ret;
		
		wdtPet();
		ret = commsRx(ci, rx, fromMac);
		
		if (ret == COMMS_RX_ERR_NO_PACKETS)
			continue;
			
		pr("RX pkt: 0x%02x + %d\r\n", rx[0], ret);
		
		if (ret == COMMS_RX_ERR_MIC_FAIL) {
			
			pr("RX: invalid MIC\r\n");
			return false;
		}
		
		if (ret < sizeof(uint8_t) + sizeof(struct PendingInfo)) {
			
			pr("RX: %d < %d\r\n", ret, sizeof(uint8_t) + sizeof(struct PendingInfo));
			return false;
		}
		
		if (rx[0] != PKT_CHECKOUT) {
			pr("RX: pkt 0x%02x @ %s\r\n", rx[0], "checkin");
			return false;
		}
		
		*out = *(struct PendingInfo*)(rx + 1);
		return true;
	}
	
	return false;
}

static void powerDownAndSleep(uint32_t msec)
{
	setPowerState(true);	//required for sleep to work
	qspiChipSleepWake(true);
	timerStop();
	einkPowerOff();
	sleepForMsec(msec);
}

static const char* fwVerString(void)
{
	static char fwVer[32] = {};
	
	if (!fwVer[0]) {
		
		sprintf(fwVer, "FW v%u.%u.%u.%u",
			(uint8_t)(SW_VER_CURRENT >> 40),
			(uint8_t)(SW_VER_CURRENT >> 32),
			(uint16_t)(SW_VER_CURRENT >> 16),
			(uint16_t)SW_VER_CURRENT);
	}
	
	return fwVer;
}

static void uiPrvFullscreenMsg(const char *str, const char *line2, const char *line3)
{
	struct CharCanvas canv = {
		.dst = displayGetFbPtr(),
		.w = DISPLAY_WIDTH,
		.h = DISPLAY_HEIGHT,
		.rowBytes = (DISPLAY_WIDTH * DISPLAY_BPP + 7) / 8,
		.bpp = DISPLAY_BPP,
	};
	
	#define LINE1_MAGN	2
	
	static const uint8_t colorA[] = {(1 << DISPLAY_BPP) - 1 /* black */, (DISPLAY_BPP == 1) ? 1 : (1 << DISPLAY_BPP) - 2 /* not blank, but less dark */, (1 << DISPLAY_BPP) - 1 /* black */};
	static const uint16_t rowA[] = {(DISPLAY_HEIGHT - CHAR_HEIGHT * LINE1_MAGN) / 2, (DISPLAY_HEIGHT - CHAR_HEIGHT * LINE1_MAGN) / 2 + CHAR_HEIGHT * LINE1_MAGN + 2, DISPLAY_HEIGHT - CHAR_HEIGHT};
	const char *strA[] = {str ? str : "", line2 ? line2 : "", line3 ? line3 : ""};
	static const bool centerA[] = {true, true, false};
	static const uint8_t magnA[] = {LINE1_MAGN, 1, 1};
	uint_fast8_t i;
	
	pr("MESSAGE: '%s', '%s', '%s'\r\n", strA[0], strA[1], strA[2]);
	
	bzero(displayGetFbPtr(), canv.rowBytes * canv.h);
	
	for (i = 0; i < 3; i++) {
		
		uint_fast8_t magn = magnA[i];
		const char *str = strA[i];
		uint_fast16_t c;
		char ch;
		
		if (centerA[i])
			c = (DISPLAY_WIDTH - CHAR_WIDTH * strlen(str) * magn) / 2;
		else
			c = 1;	//left align
		
		while ((ch = *str++) != 0) {
			
			charsDrawChar(&canv, ch, c, rowA[i], colorA[i], 0, magn);
			c += CHAR_WIDTH * magn;
		}
	}
	
	wdtPet();
	
	if (!displayRefresh(false))
		pr("display refresh failed\r\n");
}

static void uiPrvDrawBitmap(const void *data, uint32_t size)
{
	uint32_t dataOfst, w, h, s, r, c, i, bpp, effectiveH, effectiveW, clutSz, dstOft;
	struct BitmapFileHeader *bmp = (struct BitmapFileHeader*)data;
	uint8_t *clutOurs, *rowBuf = clutOurs + 256;
	uint8_t *dst = (uint8_t*)displayGetFbPtr();
	struct BitmapClutEntry *clut;
	bool bottomUp, invert;
	char *src;
	
		
	if (size < sizeof(struct BitmapFileHeader))
		return;
	
	if (bmp->headerSz < 40 || bmp->colorplanes != 1 || bmp->compression || !bmp->height || bmp->width <= 0 || bmp->bpp > 8)
		return;
	
	if (bmp->height >= 0) {
		h = bmp->height;
		bottomUp = true;
	}
	else {
		h = -bmp->height;
		bottomUp = false;
	}
	w = bmp->width;
	bpp = bmp->bpp;
	s = (w * bpp + 31) / 32 * 4;
	clutSz = bmp->numColors;
	if (!clutSz)
		clutSz = 1 << bpp;
	
	//clut does not always follow header(ask GIMP why), but it does precede data
	clut = (struct BitmapClutEntry*)(((char*)data) + bmp->dataOfst - sizeof(struct BitmapClutEntry) * clutSz);
	clutOurs = heapAlloc(clutSz);
		
	//convert clut to our understanding of color
	for (i = 0; i < clutSz; i++) {
		
		uint32_t intensity = 0;
		
		intensity += 13988 * clut[i].r;
		intensity += 47055 * clut[i].g;
		intensity +=  4750 * clut[i].b;
		
		//our colors are opposite of brightness, so we need to invert this too
		intensity ^= 0x00ffffff;
		
		clutOurs[i] = intensity >> (24 - DISPLAY_BPP);
	}
	
	src = ((char*)data) + bmp->dataOfst;
	
	effectiveH = (h > DISPLAY_HEIGHT) ? DISPLAY_HEIGHT : h;
	effectiveW = (w > DISPLAY_WIDTH) ? DISPLAY_WIDTH : w;
		
	for (r = 0; r < effectiveH; r++) {
		
		uint32_t effectiveRow = bottomUp ? effectiveH - 1 - r : r;
		uint32_t effectiveOfst = effectiveRow * s;
		uint32_t numBytes = (effectiveW * bpp + 7) / 8;
		
		//get a row
		rowBuf = src + effectiveOfst;
		
		//convert to our format
		for (c = 0; c < effectiveW; c++) {
			
			uint32_t val, inByteIdx = c * bpp / 8, inSubbyteIdx = c * bpp % 8, inBitMask = (1 << bpp) - 1, inBitIdx = 8 - bpp - inSubbyteIdx;
			uint32_t outByteIdx = c * DISPLAY_BPP / 8, outBitIdx = c * DISPLAY_BPP % 8, outBitMask = (1 << DISPLAY_BPP) - 1;
			
			//get value
			val = (rowBuf[inByteIdx] >> inBitIdx) & inBitMask;
			
			//look up in our clut
			val = clutOurs[val];
			
			//write to display
			dst[outByteIdx] &=~ (outBitMask << outBitIdx);
			dst[outByteIdx] |= val << outBitIdx;
		}
		wdtPet();
		
		dst += (DISPLAY_WIDTH * DISPLAY_BPP + 7) / 8;
	}
	
	heapFree(clutOurs);
}

static void uiPrvDrawRaw(void *data, uint32_t size)
{
	if (size > DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8)
		size = DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8;
	
	memcpy(displayGetFbPtr(), data, size);
}

static int32_t lzPrvRead(uint32_t *srcAddrP, uint32_t srcEnd)		//at least one byte of input is guaranteed to exist
{
	uint32_t val, srcAddr = *srcAddrP;
	uint8_t byte;
	
	if (srcAddr >= srcEnd)
		return -1;
	
	(void)qspiRead(0, srcAddr++, &byte, 1);
	val = byte;
	if (val >= 0xe0) {
		
		val &= 0x1f;
		val <<= 8;
		
		if (srcAddr >= srcEnd)
			return -1;
		
		(void)qspiRead(0, srcAddr++, &byte, 1);
		
		val += byte;
	}

	*srcAddrP = srcAddr;
	
	return val;
}

static uint32_t lzPrvDecompress(uint8_t *dst, uint32_t srcAddr, uint32_t srcSize, uint32_t dstSizeMax)
{
	uint32_t dstDone, srcEnd = srcAddr + srcSize;
	uint8_t marker, *dstEnd = dst + dstSizeMax, *dstStart = dst;
	
	if (srcSize <= 1 || dstSizeMax < 1)
		return 0;
	
	(void)qspiRead(0, srcAddr++, &marker, 1);
	(void)qspiRead(0, srcAddr++, dst++, 1);
	
	while (dst < dstEnd && srcAddr < srcEnd) {
		
		uint8_t byte;
		
		(void)qspiRead(0, srcAddr++, &byte, 1);
		
		if (byte != marker)		//byte copy
			*dst++ = byte;
		else if (srcAddr >= srcEnd)		//invalid but we'd better handle it
			break;
		else {
			
			(void)qspiRead(0, srcAddr, &byte, 1);
			
			if (byte == 0) {	//marker byte itself
				*dst++ = marker;
				srcAddr++;
			}
			else {					//backreference
				
				int32_t i, len, ofst;
				
				len = lzPrvRead(&srcAddr, srcEnd);
				if (len < 0)
					break;
				len += 3;
				ofst = lzPrvRead(&srcAddr, srcEnd);
				if (ofst < 0 )
					break;
				ofst++;
				
				//do not copy too much
				if (dstEnd - dst < len)
					len = dstEnd - dst;
				
				//do not allo wbackreferences past our start
				if (ofst > dst - dstStart)
					break;
				
				//copy
				for (i = 0; i < len; i++)
					dst[i] = dst[i - ofst];
				dst += i;
			}
		}
	}
	
	return dst - dstStart;
}

static void TEXT2 uiPrvDrawImageAtAddress(uint32_t addr, uint32_t size)
{
	uint8_t sig[6];
	uint8_t *data;
	
	bzero(displayGetFbPtr(), ((DISPLAY_WIDTH * DISPLAY_BPP + 7) / 8) * DISPLAY_HEIGHT);
	
	if (size < 6)	//we need enough size to even sort out what this is, that needs 6 bytes
		return;
	
	(void)qspiRead(0, addr + sizeof(struct EepromImageHeader), sig, sizeof(sig));
	wdtPet();
	
	setPowerState(true);
	
	if (sig[0] == 'L' && sig[1] == 'Z') {
		
		uint32_t decompressedSize = *(uint32_t*)(sig + 2);
		
		data = heapAlloc(decompressedSize);
		if (!data) {
			pr("alloc fail\r\n");
			return;
		}
		
		size = lzPrvDecompress(data, addr + sizeof(struct EepromImageHeader) + 6, size - 6, decompressedSize);
	}
	else {
		
		data = heapAlloc(size);
		if (!data)
			return;
		
		(void)qspiRead(0, addr + sizeof(struct EepromImageHeader), data, size);
	}
	
	wdtPet();
	
	if (data[0] == 'B' && data[1] == 'M' && data[5] == 0)
		uiPrvDrawBitmap(data, size);
	else
		uiPrvDrawRaw(data, size);
	
	heapFree(data);
	
	setPowerState(false);
	
	if (!displayRefresh(false))
		powerDownAndSleep(0);
}

static void uiPrvDrawLatestImage(const struct EepromContentsInfo *eci)
{
	if (eci->latestCompleteImgAddr)
		uiPrvDrawImageAtAddress(eci->latestCompleteImgAddr, eci->latestCompleteImgSize);
}

//prevStateP should be 0xffff for first invocation
static void prvProgressBar(uint32_t done, uint32_t outOf, uint16_t *prevStateP)
{
	uint32_t i, min, max, now = (done * (DISPLAY_WIDTH * DISPLAY_BPP / 8) + (outOf / 2)) / outOf;		//in bytes
	uint8_t *dst = displayGetFbPtr();
	bool blacken;
	
	//fill with "keep" color
	memset(dst, 0xff, DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BPP / 8);
	
	if (*prevStateP == 0xffff) {
		memset(dst, 0x00, 2 * DISPLAY_WIDTH * DISPLAY_BPP / 8);	//whiten two rows
		*prevStateP = 0;
	}
	
	if (now == *prevStateP) {
		
		min = max = 0;
	}
	else if (now < *prevStateP) {
		min = now;
		max = *prevStateP;
		blacken = false;
	}
	else {
		blacken = true;
		min = *prevStateP;
		max = now;
	}
	*prevStateP = now;
	
	memset(dst + min, blacken ? 0x55 : 0x00, max - min);
	
	displayRefresh(true);
}

static uint32_t TEXT2 prvDriveDownload(struct Settings *settings, struct CommsInfo *ci, struct EepromImageHeader *eih, uint32_t addr, bool isOS)
{
	struct {
		uint8_t pktTyp;
		struct ChunkReqInfo cri;
	} packet = {.pktTyp = PKT_CHUNK_REQ, .cri = { .osUpdatePlz = isOS, }, };
	uint8_t rx[COMMS_MAX_PACKET_SZ];
	const uint32_t nPieces = (eih->size + EEPROM_PIECE_SZ - 1) / EEPROM_PIECE_SZ;
	struct ChunkInfo *chunk = (struct ChunkInfo*)(rx + 1);
	uint8_t *data = (uint8_t*)(chunk + 1);
	bool progressMade = false;
	uint32_t curPiece;
	
	//sanity check
	if (nPieces > sizeof(eih->piecesMissing) * 8) {
		pr("DL too large: %u\r\n", eih->size);
		return settings->checkinDelay;
	}
	
	//prepare the packet
	packet.cri.versionRequested = eih->version;
	
	//find where we are in downloading
	for (curPiece = 0; curPiece < nPieces && !((eih->piecesMissing[curPiece / 8] >> (curPiece % 8)) & 1); curPiece++);
	
	pr("Requesting piece %u/%u of %s\r\n", curPiece, nPieces, isOS ? "UPDATE" : "IMAGE");
	
	//download
	for (;curPiece < nPieces; curPiece++) {
		
		uint_fast8_t now, nRetries;
		uint64_t nowStart;
		int32_t ret;
		
		//any piece that is not last will be of standard size
		if (curPiece != nPieces - 1)
			now = EEPROM_PIECE_SZ;
		else
			now = eih->size - (nPieces - 1) * EEPROM_PIECE_SZ;
		
		packet.cri.offset = curPiece * EEPROM_PIECE_SZ;
		packet.cri.len = now;
		
		if (!(((uint8_t)curPiece) & 0x3f))
			prvProgressBar(curPiece, nPieces, &settings->prevDlProgress);
		
		for (nRetries = 0; nRetries < 5; nRetries++) {
			
			commsTx(ci, false, &packet, sizeof(packet));
			
			nowStart = timerGet();
			while (1) {
				
				if (timerGet() - nowStart > TIMER_TICKS_PER_MSEC * COMMS_MAX_RADIO_WAIT_MSEC) {
					pr("RX timeout in download\r\n");
					break;
				}
				
				wdtPet();
				ret = commsRx(ci, rx, settings->masterMac);
				
				if (ret == COMMS_RX_ERR_NO_PACKETS)
					continue;	//let it time out
				else if (ret == COMMS_RX_ERR_INVALID_PACKET)
					continue;	//let it time out
				else if (ret == COMMS_RX_ERR_MIC_FAIL) {
					pr("RX: invalid MIC\r\n");
					
					//mic errors are unlikely unless someone is deliberately messing with us - check in later
					goto checkin_again;
				}
				else if ((uint8_t)ret < (uint8_t)(sizeof(uint8_t) + sizeof(struct ChunkInfo))) {
					pr("RX: %d < %d\r\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));
					
					//server glitch? check in later
					return settings->checkinDelay;
				}
				else if (rx[0] != PKT_CHUNK_RESP) {
					pr("RX: pkt 0x%02x @ %s\r\n", rx[0], "DL");
					
					//weird packet? worth retrying soner
					break;
				}
				
				//get payload len
				ret -= sizeof(uint8_t) + sizeof(struct ChunkInfo);
				
				if (chunk->osUpdatePlz != isOS) {
					pr("RX: wrong data type @ DL: %d\r\n", chunk->osUpdatePlz);
					continue;	//could be an accidental RX of older packet - ignore
				}
				else if (chunk->offset != packet.cri.offset) {
					pr("RX: wrong offset @ DL 0x%08lx != 0x%08lx\r\n", chunk->offset, packet.cri.offset);
					continue;	//could be an accidental RX of older packet - ignore
				}
				else if (!ret) {
					pr("RX: DL no longer avail\r\n");
					
					//just check in later
					goto checkin_again;
				}
				else if (ret != packet.cri.len) {
					
					pr("RX: Got %ub, reqd %u\r\n", ret, packet.cri.len);
					
					//server glitch? check in later
					goto checkin_again;
				}
				
				//write data
				qspiWrite(false, addr + curPiece * EEPROM_PIECE_SZ + sizeof(struct EepromImageHeader), data, ret);
				
				//write marker
				eih->piecesMissing[curPiece / 8] &=~ (1 << (curPiece % 8));
				qspiWrite(false, addr + offsetof(struct EepromImageHeader, piecesMissing[curPiece / 8]), &eih->piecesMissing[curPiece / 8], 1);
				
				progressMade = true;
				nRetries = 100;	//so we break the loop
				break;
			}
		}
		if (nRetries == 5) {
			pr("retried too much\r\n");
			if (progressMade)
				goto retry_later;
			else
				goto checkin_again;
		}
	}
	
downloadDone:
	pr("Done at piece %u/%u\r\n", curPiece, nPieces);
	radioShutdown();
	
	//if we are here, we succeeded in finishing the download	
	eih->validMarker = EEPROM_IMG_VALID;
	qspiWrite(false, addr + offsetof(struct EepromImageHeader, validMarker), &eih->validMarker, sizeof(eih->validMarker));
	
	pr("DL completed\r\n");
	prvProgressBar(nPieces, nPieces, &settings->prevDlProgress);
	settings->prevDlProgress = 0xffff;
	
	//act on it
	if (!isOS)
		uiPrvDrawImageAtAddress(addr, eih->size);
	else if (prvApplyUpdateIfNeeded()) {
		pr("reboot post-update\r\n");
		return 100;
	}
	//fallthrough
	
checkin_again:
	if (curPiece != nPieces)
		prvProgressBar(curPiece, nPieces, &settings->prevDlProgress);
	return settings->checkinDelay;

retry_later:
	prvProgressBar(curPiece, nPieces, &settings->prevDlProgress);
	return settings->retryDelay;
}

static void prvWriteNewHeader(struct EepromImageHeader *eih, uint32_t addr, uint32_t eeSize, uint64_t ver, uint32_t size)
{
	qspiEraseRange(addr, eeSize);
	
	bzero(eih, sizeof(struct EepromImageHeader));
	eih->version = ver;
	eih->validMarker = EEPROM_IMG_INPROGRESS;
	eih->size = size;
	memset(eih->piecesMissing, 0xff, sizeof(eih->piecesMissing));
	
	qspiWrite(false, addr, eih, sizeof(struct EepromImageHeader));
}

static uint32_t prvDriveUpdateDownload(struct Settings *settings, struct CommsInfo *ci, uint64_t ver, uint32_t size)
{
	struct EepromImageHeader *eih = (struct EepromImageHeader*)heapAlloc(sizeof(struct EepromImageHeader));
	uint32_t ret;
	
	//see what's there already
	qspiRead(0, EEPROM_UPDATE_START, eih, sizeof(struct EepromImageHeader));
	if (eih->version != ver)
		prvWriteNewHeader(eih, EEPROM_UPDATE_START, EEPROM_UPDATE_LEN, ver, size);
	
	ret = prvDriveDownload(settings, ci, eih, EEPROM_UPDATE_START, true);
	
	heapFree(eih);
	return ret;
}

static uint32_t prvDriveImageDownload(struct Settings *settings, struct CommsInfo *ci, const struct EepromContentsInfo *eci, uint64_t ver, uint32_t size)
{
	struct EepromImageHeader *eih = (struct EepromImageHeader*)heapAlloc(sizeof(struct EepromImageHeader));
	uint32_t addr, ret;
	
	//sort out where next image should live
	if (eci->latestInprogressImgAddr)
		addr = eci->latestInprogressImgAddr;
	else if (!eci->latestCompleteImgAddr)
		addr = EEPROM_IMG_START;
	else {
		addr = eci->latestCompleteImgAddr + EEPROM_IMG_EACH;
		if (addr >= EEPROM_IMG_START + EEPROM_IMG_LEN)
			addr = EEPROM_IMG_START;
	}
	
	//see what's there already
	qspiRead(0, addr, eih, sizeof(struct EepromImageHeader));
	if (eih->version != ver)
		prvWriteNewHeader(eih, addr, EEPROM_IMG_EACH, ver, size);
	
	ret = prvDriveDownload(settings, ci, eih, addr, false);
	
	heapFree(eih);
	return ret;
}

static void radioInitialize(void)
{
	radioEarlyInit();
	radioRxQueueInit();
	radioLoadCalibData();
	radioSetFilterShortAddr(0xffff);
	radioSetFilterLongAddr(mSelfMac);
	radioSetFilterPan(PROTO_PAN_ID);
	radioRxFilterEnable(true);
}

static uint32_t uiPaired(struct Settings *settings, struct CommsInfo *ci)
{
	struct EepromContentsInfo eci;
	uint32_t addr, ofst, i;
	struct PendingInfo pi;

	//do this before we turn on the radio, for power reasons
	prvEepromIndex(&eci);
	
	radioInitialize();
	radioSetChannel(SETTING_CHANNEL_OFFSET + settings->channel);
	radioSetTxPower(40);
	
	//try five times
	for (i = 0; i < 5 && !prvSendCheckin(settings, ci, &pi); i++);
	
	if (i == 5) {	//fail
		
		radioShutdown();
		pr("checkin fails\r\n");
		settings->numFailedCheckins++;
		
		if (settings->failedCheckinsTillDissoc && settings->numFailedCheckins == settings->failedCheckinsTillDissoc) {
			pr("Disassoc as %u = %u\r\n", settings->numFailedCheckins, settings->failedCheckinsTillDissoc);
			settings->isPaired = 0;
			
			return 1000;	//wake up in a second to try to pair
		}
		
		if (settings->failedCheckinsTillBlank && settings->numFailedCheckins == settings->failedCheckinsTillBlank) {
			pr("Blank as %u = %u\r\n", settings->numFailedCheckins, settings->failedCheckinsTillBlank);
			uiPrvFullscreenMsg("NO SIGNAL FOR TOO LONG", NULL, fwVerString());
		}
		
		//try again in due time
		return settings->checkinDelay;
	}
	
	//if we got here, we succeeded with the check-in. if screen was blanked, redraw it
	if (settings->failedCheckinsTillBlank && settings->numFailedCheckins >= settings->failedCheckinsTillBlank)
		uiPrvDrawLatestImage(&eci);
	settings->numFailedCheckins = 0;
	
	pr("Base: %s ver 0x%08x%08x, us 0x%08x%08x\r\n", " OS",
		(uint32_t)(pi.osUpdateVer >> 32), (uint32_t)pi.osUpdateVer,
		(uint32_t)(SW_VER_CURRENT >> 32), (uint32_t)SW_VER_CURRENT);
	
	pr("Base: %s ver 0x%08x%08x, us 0x%08x%08x\r\n", "IMG",
		(uint32_t)(pi.imgUpdateVer >> 32), (uint32_t)pi.imgUpdateVer,
		(uint32_t)(eci.latestCompleteImgVer >> 32), (uint32_t)eci.latestCompleteImgVer);
	
	//if there is an update, we want it
	if ((pi.osUpdateVer & VERSION_SIGNIFICANT_MASK) > (SW_VER_CURRENT & VERSION_SIGNIFICANT_MASK))
		return prvDriveUpdateDownload(settings, ci, pi.osUpdateVer, pi.osUpdateSize);
	
	if (pi.imgUpdateVer > eci.latestCompleteImgVer)
		return prvDriveImageDownload(settings, ci, &eci, pi.imgUpdateVer, pi.imgUpdateSize);
	
	//nothing? guess we'll check again later
	return settings->checkinDelay;
}

static uint32_t uiNotPaired(struct Settings *settings, struct CommsInfo *ci)
{
	struct {
		uint8_t pktType;
		struct TagInfo ti;
	} __attribute__((packed)) packet = {};
	uint8_t rx[COMMS_MAX_PACKET_SZ];
	uint64_t waitEnd, nowStart;
	uint_fast8_t i, ch;
	char macStr[32];
	int32_t ret;
	
	packet.pktType = PKT_ASSOC_REQ;
	packet.ti.protoVer = PROTO_VER_CURRENT;
	prvFillTagState(settings, &packet.ti.state);
	packet.ti.screenPixWidth = 400;
	packet.ti.screenPixHeight = 300;
	packet.ti.screenMmWidth = 84;
	packet.ti.screenMmHeight = 63;
	packet.ti.compressionsSupported = PROTO_COMPR_TYPE_LZ;
	packet.ti.maxWaitMsec = COMMS_MAX_RADIO_WAIT_MSEC;
	
	switch (DISPLAY_BPP) {
		case 1:		packet.ti.screenType = TagScreenEink_BW_1bpp;	break;
		case 2:		packet.ti.screenType = TagScreenEink_BW_2bpp;	break;
		default:	packet.ti.screenType = TagScreenTypeOther;		break;
	}
	
	sprintf(macStr, "("MACFMT")", MACCVT(mSelfMac));
	uiPrvFullscreenMsg("READY TO ASSOCIATE", macStr, fwVerString());
	
	
	radioInitialize();
	radioSetTxPower(0);
	
	for (ch = 11; ch <= 26; ch++) {
		
		pr("try ch %u\r\n", ch);
		radioSetChannel(ch);
		
		prvProgressBar(ch - 11, 26 - 11 + 1, &settings->prevDlProgress);
		
		waitEnd = timerGet() + TIMER_TICKS_PER_SEC / 2;	//try for 1/2 second per channel
		while (timerGet() < waitEnd) {
			
			commsTx(ci, true, &packet, sizeof(packet));
			nowStart = timerGet();
			while (timerGet() - nowStart < TIMER_TICKS_PER_MSEC * 150 /* wait 150 ms before retransmitting */) {
				
				wdtPet();
				ret = commsRx(ci, rx, settings->masterMac);
				
				if (ret != COMMS_RX_ERR_NO_PACKETS)
					pr("RX pkt: 0x%02x + %d\r\n", rx[0], ret);
				
				if (ret == COMMS_RX_ERR_MIC_FAIL)
					pr("RX: invalid MIC\r\n");
				else if (ret <= 0)
					;	//nothing
				else if (ret < sizeof(uint8_t) + sizeof(struct AssocInfo))
					pr("RX: %d < %d\r\n", ret, sizeof(uint8_t) + sizeof(struct AssocInfo));
				else if (rx[0] != PKT_ASSOC_RESP)
					pr("RX: pkt 0x%02x @ %s\r\n", rx[0], "pair");
				else if (commsGetLastPacketRSSI() < -50)
					pr("RX: too weak to associate: %d\n", commsGetLastPacketRSSI());
				else {
					struct AssocInfo *ai = (struct AssocInfo*)(rx + 1);
					
					settings->checkinDelay = ai->checkinDelay;
					settings->retryDelay = ai->retryDelay;
					settings->failedCheckinsTillBlank = ai->failedCheckinsTillBlank;
					settings->failedCheckinsTillDissoc = ai->failedCheckinsTillDissoc;
					settings->channel = ch - SETTING_CHANNEL_OFFSET;
					settings->numFailedCheckins = 0;
					settings->nextIV = 0;
					memcpy(settings->encrKey, ai->newKey, sizeof(settings->encrKey));
					settings->isPaired = 1;
					
					radioShutdown();
					
					pr("Associated to master "MACFMT"\r\n", MACCVT(settings->masterMac));
					
					settings->prevDlProgress = 0xffff;
					uiPrvFullscreenMsg("\x01\x02", NULL, fwVerString());	//signal icon
					
					wdtPet();
					
					pr("Erz IMG\r\n");
					qspiEraseRange(EEPROM_IMG_START, EEPROM_IMG_LEN);
					
					wdtPet();
					
					pr("Erz UPD\r\n");
					qspiEraseRange(EEPROM_UPDATE_START, EEPROM_UPDATE_LEN);
					
					return 1000;	//wake up in a second to check in
				}
			}
		}
	}
	
	radioShutdown();
	settings->prevDlProgress = 0xffff;
	uiPrvFullscreenMsg("\x03\x04", NULL, fwVerString());	//no signal icon
	
	return 0;	//never wake up
}

static void hwInit(void)
{
	PMU->POWERDOWN = MZ_PMU_POWERDOWN_3DGLASS | MZ_PMU_POWERDOWN_IR | MZ_PMU_POWERDOWN_KEYSCAN | MZ_PMU_POWERDOWN_DMA |
						MZ_PMU_POWERDOWN_I2C1 | MZ_PMU_POWERDOWN_I2C2 | MZ_PMU_POWERDOWN_UART1 |
						MZ_PMU_POWERDOWN_GPT2 | MZ_PMU_POWERDOWN_SSP1 | MZ_PMU_POWERDOWN_CLKOUT;
	
	timerInit();
}

static void prvGetSelfMac(void)
{
	(void)qspiRead(0, EEPROM_MAC_INFO_START, mSelfMac, 8);
	
	if ((((uint32_t*)mSelfMac)[0] | ((uint32_t*)mSelfMac)[1]) == 0 || (((uint32_t*)mSelfMac)[0] & ((uint32_t*)mSelfMac)[1]) == 0xffffffff) {	//fastest way to check for all ones or all zeroes
		
		pr("mac unknown\r\n");
		powerDownAndSleep(0);
	}
}

static void showVersionAndVerifyMatch(void)
{
	extern uint32_t versionEePos[];
	uint64_t localVer;
	
	//the &mCurVersionExport access is necessary to make sure mCurVersionExport is referenced
	pr("Booting FW ver 0x%08x%08x (at 0x%08x, exported at EE 0x%08x)\r\n",
		(unsigned)(SW_VER_CURRENT >> 32), (unsigned)SW_VER_CURRENT, &mCurVersionExport, versionEePos);
	if ((uint32_t)versionEePos != HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST) {
		pr("ver loc mismatch\r\n");
		powerDownAndSleep(0);
	}
	
	(void)qspiRead(0, (uint32_t)versionEePos, &localVer, sizeof(localVer));
	if (localVer != SW_VER_CURRENT) {
		pr("ver val mismatch\r\n");
		powerDownAndSleep(0);
	}
	
	if (SW_VER_CURRENT &~ VERSION_SIGNIFICANT_MASK) {
		pr("ver num @ red zone\r\n");
		powerDownAndSleep(0);
	}
}

static void wdtReconfig(void)
{
	wdtPet();
	WDT->CR = 0x1d;		//long reset period, reset on timeout, enabled
	WDT->TORR = 9;
	wdtPet();
}

void main(void)
{
	struct Settings settings;
	uint32_t sleepDuration;
	struct CommsInfo ci;
	
	wdtReconfig();
	showVersionAndVerifyMatch();
	hwInit();
	heapInit();
	wdtPet();
	displayInit();
	wdtPet();
	settingsRead(&settings);
	wdtPet();
	
	
	prvGetSelfMac();
	
	setPowerState(false);
	
	ci.myMac = mSelfMac;
	ci.masterMac = settings.masterMac;
	ci.encrKey = settings.encrKey;
	ci.nextIV = &settings.nextIV;
	
	wdtPet();

	if (settings.isPaired) {
		
		
		if (prvApplyUpdateIfNeeded()) {
			
			pr("reboot post-update\r\n");
			sleepDuration = 100;	//reboot for a little bit
		}
		else {
			
			sleepDuration = uiPaired(&settings, &ci);
		}
	}
	else {
		
		static const uint32_t presharedKey[] = PROTO_PRESHARED_KEY;
		
		memcpy(settings.encrKey, presharedKey, sizeof(settings.encrKey));
		sleepDuration = uiNotPaired(&settings, &ci);
	}
	
	settings.lastRxedLQI = commsGetLastPacketLQI();
	settings.lastRxedRSSI = commsGetLastPacketRSSI();
	
	settingsWrite(&settings);
	
	//vary sleep a little (6%-ish) to avoid repeated collisions
	sleepDuration = ((uint64_t)sleepDuration * (((rnd32() & 31) - 15) + 256)) / 256;
	
	pr("sleep: %u\r\n", sleepDuration);
	powerDownAndSleep(sleepDuration);
	while(1);
}

