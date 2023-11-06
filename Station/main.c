#include "nrf52840_bitfields.h"
#include "../patchedfw/proto.h"
#include "timebase.h"
#include "imgStore.h"
#include "nrf52840.h"
#include <stdbool.h>
#include "tiRadio.h"
#include <string.h>
#include <stdint.h>
#include "printf.h"
#include "radio.h"
#include "comms.h"
#include "led.h"
#include "usb.h"
#include "aes.h"

//maybe add cache of known tags and their last known info, export as RO file on another LUN



static uint32_t mRootKey[4];
static bool mHaveSubGhzRadio;


void prPutchar(char chr)
{
	volatile uint32_t *word = (volatile uint32_t*)0x2003fffc;
	static bool mDisabled = false;
	uint32_t ctr;
	
	if (mDisabled)
		return;
	ctr = 5000000;
	while (*word & 0x80000000) {
		if (!--ctr) {
			mDisabled = true;
			return;
		}
	}
	*word = 0x80000000 | (uint8_t)chr;
}

static void prvRandomBytes(void *dstP, uint32_t num)
{
	uint8_t *dst = (uint8_t*)dstP;
	
	NRF_RNG->TASKS_STOP = 1;
	(void)NRF_RNG->TASKS_STOP;
	NRF_RNG->SHORTS = 0;
	NRF_RNG->CONFIG = 1;
	NRF_RNG->TASKS_START = 1;
	
	while (num--) {
		NRF_RNG->EVENTS_VALRDY = 0;
		while (!NRF_RNG->EVENTS_VALRDY);
		*dst++ = NRF_RNG->VALUE;
	}
	
	NRF_RNG->TASKS_STOP = 1;
}

static void hwPrvVerifyRegout(void)
{
	if (((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) >> UICR_REGOUT0_VOUT_Pos) != UICR_REGOUT0_VOUT_3V3)
	{
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
		NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~UICR_REGOUT0_VOUT_Msk) | (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);
		NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
		while (NRF_NVMC->READY == NVMC_READY_READY_Busy);
	
		pr("setting vregout");
	
		NVIC_SystemReset();
	}
}

static void hwInit(void)
{
	//none of the errata in there affect us, so let's save on the code size and skip this nonsense
	SystemInit();
	
	//verify regulator is at 3.3v, if not, set it and reset
	hwPrvVerifyRegout();

	//init and switch to HF crystal
	NRF_CLOCK->TASKS_HFCLKSTART = 1;
	while (!(NRF_CLOCK->HFCLKSTAT & (CLOCK_HFCLKSTAT_STATE_Running << CLOCK_HFCLKSTAT_STATE_Pos)));
	while (((NRF_CLOCK->HFCLKSTAT >> CLOCK_HFCLKSTAT_STATE_Pos) & CLOCK_HFCLKSTAT_SRC_Msk) != CLOCK_HFCLKSTAT_SRC_Xtal);

	//timebase on
	timebaseInit();
	
	//config PWM
	ledInit();
	
	asm volatile("cpsie i");
}

static void cryptoInit(uint8_t *myMac)
{
	uint32_t mixKey[4] = {}, mixPlaintext[4] = {}, mixCypher[4];
	
	//create a stable but unpredictable root key
	mixKey[0] = NRF_FICR->DEVICEID[0];
	mixKey[1] = NRF_FICR->DEVICEID[1];
	mixKey[2] = NRF_FICR->DEVICEADDR[0];
	mixKey[3] = NRF_FICR->DEVICEADDR[1];
	mixPlaintext[0] = (NRF_FICR->TEMP.A0 << 16) | NRF_FICR->TEMP.A1;
	mixPlaintext[1] = (NRF_FICR->TEMP.A2 << 16) | NRF_FICR->TEMP.A3;
	mixPlaintext[2] = (NRF_FICR->TEMP.A4 << 16) | NRF_FICR->TEMP.A5;
	mixPlaintext[3] = (NRF_FICR->TEMP.B0 << 16) | NRF_FICR->TEMP.B1;
	aesEnc(mixKey, mixPlaintext, mixCypher);
	mixPlaintext[0] = mixCypher[0] ^ ((NRF_FICR->TEMP.B2 << 16) | NRF_FICR->TEMP.B3);
	mixPlaintext[1] = mixCypher[0] ^ ((NRF_FICR->TEMP.B4 << 16) | NRF_FICR->TEMP.B5);
	mixPlaintext[2] = mixCypher[0] ^ ((NRF_FICR->TEMP.T0 << 16) | NRF_FICR->TEMP.T1);
	mixPlaintext[3] = mixCypher[0] ^ ((NRF_FICR->TEMP.T2 << 16) | NRF_FICR->TEMP.T3);
	aesEnc(mixKey, mixPlaintext, mRootKey);
	
	//now also create mac
	mixPlaintext[0] = NRF_FICR->NFC.TAGHEADER0;
	mixPlaintext[1] = NRF_FICR->NFC.TAGHEADER1;
	mixPlaintext[2] = NRF_FICR->NFC.TAGHEADER2;
	mixPlaintext[3] = NRF_FICR->NFC.TAGHEADER3;
	aesEnc(mixKey, mixPlaintext, mixCypher);
	
	
	//make a mac. Last 3 bytes are VENDOR, we use 4
	myMac[7] = 'D';
	myMac[6] = '.';
	myMac[5] = 'G';
	myMac[4] = '.';
	*(uint32_t*)myMac = mixCypher[0] ^ mixCypher[1] ^ mixCypher[2] ^ mixCypher[3];
	
	pr("Decided on a MAC "MACFMT"\n", MACCVT(myMac));
	pr("Root key is 0x%08x%08x%08x%08x\n", (unsigned)mRootKey[0], (unsigned)mRootKey[1], (unsigned)mRootKey[2], (unsigned)mRootKey[3]);
}

void commsExtKeyForMac(uint32_t *key, const uint8_t *mac)
{
	key[2] = key[0] = *(const uint32_t*)(mac + 0);
	key[3] = key[1] = *(const uint32_t*)(mac + 4);
	
	aesEnc(mRootKey, key, key);
}

static void prvProcessAssocReq(uint8_t radioIdx, const uint8_t *fromMac, int_fast8_t rssi, uint_fast8_t lqi, const struct TagInfo* info)
{
	struct {
		uint8_t pktTyp;
		struct AssocInfo ai;
	} __attribute__((packed)) ai = {};
	bool ok;
	
	pr("TAG requesting association (LQi %u, RSSI %d):\n", lqi, rssi);
	pr(" Radio:               %u\n", radioIdx);
	pr(" MAC:                 "MACFMT"\n", MACCVT(fromMac));
	pr(" Protocol version:    %u\n", info->protoVer);
	if (info->protoVer != PROTO_VER_CURRENT)
		return;
	pr(" HW type:             %u\n", info->state.hwType);
	pr(" SW version:          v%u.%u.%u.%u (0x%016llx)\n",
		(unsigned)(info->state.swVer >> 40) & 0xff, (unsigned)(info->state.swVer >> 32) & 0xff,
		(unsigned)(info->state.swVer >> 16) & 0xffff, (unsigned)(info->state.swVer >> 0) & 0xffff,
		info->state.swVer);
	pr(" Battery:             %u.%03uV\n", info->state.batteryMv / 1000, info->state.batteryMv % 1000);
	pr(" Screen Pixels:       %u x %u\n", info->screenPixWidth, info->screenPixHeight);
	pr(" Screen Size (mm):    %u x %u\n", info->screenMmWidth, info->screenMmHeight);
	pr(" Compression support: 0x%04x\n", info->compressionsSupported);
	pr(" Max wake time (ms):  %u\n", info->maxWaitMsec);
	pr(" Screen type:         ");
	switch (info->screenType) {
		case TagScreenEink_BW_1bpp:			pr("eInk, B&W, %ubpp\n", 1);				break;
		case TagScreenEink_BW_2bpp:			pr("eInk, B&W, %ubpp\n", 2);				break;
		case TagScreenEink_BW_3bpp:			pr("eInk, B&W, %ubpp\n", 3);				break;
		case TagScreenEink_BW_4bpp:			pr("eInk, B&W, %ubpp\n", 4);				break;
		case TagScreenEink_BWY_only:		pr("eInk, BW%c, %ubpp\n", 'Y', 1);			break;
		case TagScreenEink_BWY_2bpp:		pr("eInk, BW%c, %ubpp\n", 'Y', 2);			break;
		case TagScreenEink_BWY_3bpp:		pr("eInk, BW%c, %ubpp\n", 'Y', 3);			break;
		case TagScreenEink_BWY_4bpp:		pr("eInk, BW%c, %ubpp\n", 'Y', 4);			break;
		case TagScreenEink_BWR_only:		pr("eInk, BW%c, %ubpp\n", 'R', 1);			break;
		case TagScreenEink_BWR_2bpp:		pr("eInk, BW%c, %ubpp\n", 'R', 2);			break;
		case TagScreenEink_BWR_3bpp:		pr("eInk, BW%c, %ubpp\n", 'R', 3);			break;
		case TagScreenEink_BWR_4bpp:		pr("eInk, BW%c, %ubpp\n", 'R', 4);			break;
		case TagScreenPersistentLcd_1bpp:	pr("PersistLCD, 1bpp\n");					break;
		case TagScreenEink_BWY_5colors:		pr("eInk, BW%c, %u colors\n", 'Y', 5);		break;
		case TagScreenEink_BWR_5colors:		pr("eInk, BW%c, %u colors\n", 'R', 5);		break;
		case TagScreenEink_BWY_6colors:		pr("eInk, BW%c, %u colors\n", 'Y', 6);		break;
		case TagScreenEink_BWR_6colors:		pr("eInk, BW%c, %u colors\n", 'R', 6);		break;
		default:							pr("UNKNOWN 0x%02x\n", info->screenType);	break;
	};

	if (rssi < -75) {
		pr("refusing association due to low RSSI (%d)\n", rssi);
		return;
	}

	pr("ACCEPTING tag...\n");
	ai.pktTyp = PKT_ASSOC_RESP;
	ai.ai.checkinDelay = 3600000;		//check in once an hour
	ai.ai.retryDelay = 1000;			//retry in a second for failures
	ai.ai.failedCheckinsTillBlank = 4;
	ai.ai.failedCheckinsTillDissoc = 16;
	commsExtKeyForMac(ai.ai.newKey, fromMac);
	
//	ai.ai.failedCheckinsTillBlank = 2;		//XXX: 
//	ai.ai.failedCheckinsTillDissoc = 3;		//XXX: 
//	ai.ai.checkinDelay = 10000;				//XXX: check in once every 10 seconds
	
	pr(" provisioning key %08x %08x %08x %08x\n", (unsigned)ai.ai.newKey[0], (unsigned)ai.ai.newKey[1], (unsigned)ai.ai.newKey[2], (unsigned)ai.ai.newKey[3]);
	ok = commsTx(radioIdx, fromMac, &ai, sizeof(ai), true, true, true);
	pr(" done: %s\n", ok ? "OK" : "NO ACK RXed");
}

static void prvProcessCheckin(uint8_t radioIdx, const uint8_t *fromMac, struct CheckinInfo *checkin, int8_t rssi, uint8_t lqi)
{
	struct {
		uint8_t packetTyp;
		struct PendingInfo pi;
	} __attribute__((packed)) packet = {};
	uint32_t imgLen, updLen;
	uint64_t imgRev, updRev;
	uint32_t updtHandle;
	
	if (!imgStoreGetImgInfo(fromMac, &imgRev, &imgLen, NULL))
		imgRev = imgLen = 0;
	
	if (!imgStoreGetUpdateInfo(checkin->state.hwType, &updRev, &updLen, &updtHandle))
		updRev = updLen = 0;
	else {
		//when we get a download request, it does not have hwType which we need. So we hide it here
		updRev = (updRev & 0x0000ffffffffffffull) | (((uint64_t)checkin->state.hwType) << 48);
	}
	
	packet.packetTyp = PKT_CHECKOUT;
	
	packet.pi.imgUpdateVer = imgRev;
	packet.pi.imgUpdateSize = imgLen;
	packet.pi.osUpdateVer = updRev;
	packet.pi.osUpdateSize = updLen;
	
	commsTx(radioIdx, fromMac, &packet, sizeof(packet), false, false, false);
	
	
	pr("TAG checkin (LQI: %u, RSSI: %d)\n", lqi, rssi);
	pr(" Radio:               %u\n", radioIdx);
	pr(" MAC:                 "MACFMT"\n", MACCVT(fromMac));
	pr(" HW type:             %u\n", checkin->state.hwType);
	pr(" SW version:          v%u.%u.%u.%u (0x%016llx)\n",
		(unsigned)(checkin->state.swVer >> 40) & 0xff, (unsigned)(checkin->state.swVer >> 32) & 0xff,
		(unsigned)(checkin->state.swVer >> 16) & 0xffff, (unsigned)(checkin->state.swVer >> 0) & 0xffff,
		checkin->state.swVer);
	pr(" Battery:             %u.%03uV\n", checkin->state.batteryMv / 1000, checkin->state.batteryMv % 1000);
	if (checkin->lastPacketLQI)
		pr(" RxLQI:               %u\n", checkin->lastPacketLQI);
	if (checkin->lastPacketRSSI)
		pr(" RxRSSI:              %d\n", checkin->lastPacketRSSI);
	if (checkin->temperature)
		pr(" Temperature:         %d C\n", checkin->temperature - CHECKIN_TEMP_OFFSET);
}

static void prvProcessChunkReq(uint8_t radioIdx, const uint8_t *fromMac, struct ChunkReqInfo *req)
{
	uint32_t fileLen, handle;
	uint64_t fileRev;
	
	struct {
		uint8_t packetTyp;
		struct ChunkInfo chunk;
		uint8_t data[PROTO_MAX_DL_LEN];
	} __attribute__((packed)) packet = {};
	
	packet.packetTyp = PKT_CHUNK_RESP;
	packet.chunk.offset = req->offset;
	packet.chunk.osUpdatePlz = req->osUpdatePlz;
	
	if (req->osUpdatePlz) {
		
		uint32_t hwType = req->versionRequested >> 48;	//we hid it there when advertising the update
		uint64_t updateRev = req->versionRequested & VERSION_SIGNIFICANT_MASK;
		
		//if we cnanot fullfil the entire request, fulfill none
		if (!imgStoreGetUpdateInfo(hwType, &fileRev, &fileLen, &handle) || (fileRev & VERSION_SIGNIFICANT_MASK) != (updateRev & VERSION_SIGNIFICANT_MASK) || req->offset >= fileLen || fileLen - req->offset < req->len || req->len > PROTO_MAX_DL_LEN)
			return;
		
		imgStoreGetFileData(packet.data, handle, req->offset, req->len);
		pr("tx %ub UPD @ %u h: %d\n", (unsigned)req->len, (unsigned)req->offset, (unsigned)handle);
	}
	else {
		
		if (!imgStoreGetImgInfo(fromMac, &fileRev, &fileLen, &handle) || fileRev != req->versionRequested || req->offset >= fileLen || fileLen - req->offset < req->len || req->len > PROTO_MAX_DL_LEN)
			return;
		
		//if we cannot fulfill the entire request, fulfill none
		imgStoreGetFileData(packet.data, handle, req->offset, req->len);
		pr("tx %ub IMG @ %u h: %d\n", (unsigned)req->len, (unsigned)req->offset, (unsigned)handle);
	}
	
	commsTx(radioIdx, fromMac, &packet, sizeof(packet) - PROTO_MAX_DL_LEN + req->len, false, false, false);
}

int main(void)
{
	uint8_t myMac[8];
	uint32_t ivStart;
	
	pr("in entry\n");
	
	hwInit();
	
	imgStoreInit();
	
	cryptoInit(myMac);
	prvRandomBytes(&ivStart, sizeof(ivStart));
	
	commsInit(myMac, ivStart);
	if (!radioInit()) {
		pr("radio init fail\n");
		while(1);
	}
	
	if (!tiRadioInit())
		pr("Sub-GHz radio init failed\n");
	else if (!tiRadioSetChannel(100))
		pr("SubGHz radio channel fail\n");
	else
		mHaveSubGhzRadio = true;
	
	if (!radioSetChannel(20)) {
		pr("radio channel fail\n");
		while(1);
	}

	radioRxFilterCfg(myMac, SHORT_MAC_UNUSED, PROTO_PAN_ID, false);
	if (mHaveSubGhzRadio)
		tiRadioRxFilterCfg(myMac, SHORT_MAC_UNUSED, PROTO_PAN_ID, false);
	radioTxConfigure(myMac, SHORT_MAC_UNUSED, PROTO_PAN_ID);
	if (mHaveSubGhzRadio)
		tiRadioTxConfigure(myMac, SHORT_MAC_UNUSED, PROTO_PAN_ID);
	radioRxEnable(true, true);
	if (mHaveSubGhzRadio)
		tiRadioRxEnable(true, true);
	
	ledSet(-1, 10, -1, -1);
	
	while(1) {
		
		uint8_t packet[COMMS_MAX_PACKET_SZ], from[8], lqi, radioIdx;
		bool pktWasBroadcast;
		int32_t ret;
		int8_t rssi;
		
		imgStoreProcess();
		
		ret = commsRx(1 | (mHaveSubGhzRadio ? 2 : 0), &radioIdx, packet, from, &rssi, &lqi, &pktWasBroadcast);
		if (ret > 0) {
			
			//pr("PKT: "MACFMT"(LQ%u RS%d) -> r#%x\n  ", MACCVT(from), lqi, rssi, radioIdx);
			
			if (pktWasBroadcast) {
				if (packet[0] != PKT_ASSOC_REQ)
					pr("message type 0x%02x not expected on the preshared channel\n", packet[0]);
				else if (ret != sizeof(struct TagInfo) + 1)
					pr("ASSOC_REQ size improper\n");
				else if (!(((uint32_t*)from)[0] | ((uint32_t*)from)[1]))
					pr("ASSOC_REQ with no return address\n");
				else
					prvProcessAssocReq(radioIdx, from, rssi, lqi, (struct TagInfo*)(packet + 1));
			}
			else switch (packet[0]) {
				
				case PKT_CHECKIN:
					if (ret != 1 + sizeof(struct CheckinInfo))
						pr("CHECKIN size improper\n");
					else
						prvProcessCheckin(radioIdx, from, (struct CheckinInfo*)(packet + 1), rssi, lqi);
					break;
				
				case PKT_CHUNK_REQ:
					if (ret != 1 + sizeof(struct ChunkReqInfo))
						pr("CHUNK_REQ size improper\n");
					else
						prvProcessChunkReq(radioIdx, from, (struct ChunkReqInfo*)(packet + 1));
					break;
				
				default:
					pr("not sure about packet 0x%02x\n", packet[0]);
					break;
			}
		}
	}
	
	
	while(1);
}



