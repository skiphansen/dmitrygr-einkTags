#include <string.h>
#include "comms.h"
#include "proto.h"
#include "fw.h"

#define ADDR_MODE_NONE					(0)
#define ADDR_MODE_SHORT					(2)
#define ADDR_MODE_LONG					(3)

#define FRAME_TYPE_BEACON				(0)
#define FRAME_TYPE_DATA					(1)
#define FRAME_TYPE_ACK					(2)
#define FRAME_TYPE_MAC_CMD				(3)


struct {
	struct TxRequest txReq;
	uint8_t packet[128];
} static mCommsBuf = {};
static uint8_t mSeq = 0;
static uint8_t mLastLqi = 0;
static int8_t mLastRSSI = 0;


struct MacFcs {
	uint16_t frameType			: 3;
	uint16_t secure				: 1;
	uint16_t framePending		: 1;
	uint16_t ackReqd			: 1;
	uint16_t panIdCompressed	: 1;
	uint16_t rfu1				: 3;
	uint16_t destAddrType		: 2;
	uint16_t frameVer			: 2;
	uint16_t srcAddrType		: 2;
} __attribute__((packed));

struct MacFrameFromMaster {
	struct MacFcs fcs;
	uint8_t seq;
	uint16_t pan;
	uint8_t dst[8];
	uint16_t from;
} __attribute__((packed));

struct MacFrameNormal {
	struct MacFcs fcs;
	uint8_t seq;
	uint16_t pan;
	uint8_t dst[8];
	uint8_t src[8];
} __attribute__((packed));

struct MacFrameBcast {
	struct MacFcs fcs;
	uint8_t seq;
	uint16_t dstPan;
	uint16_t dstAddr;
	uint16_t srcPan;
	uint8_t src[8];
} __attribute__((packed));

uint8_t commsGetLastPacketLQI(void)
{
	return mLastLqi;
}

int8_t commsGetLastPacketRSSI(void)
{
	return mLastRSSI;
}

static inline void __attribute__((always_inline)) macCopy(uint8_t *restrict dst, const uint8_t *restrict src)
{
	((uint32_t*)dst)[0] = ((const uint32_t*)src)[0];
	((uint32_t*)dst)[1] = ((const uint32_t*)src)[1];
}

static inline bool __attribute__((always_inline)) macIsEq(const uint8_t *restrict dst, const uint8_t *restrict src)
{
	return ((uint32_t*)dst)[0] == ((const uint32_t*)src)[0] && ((uint32_t*)dst)[1] == ((const uint32_t*)src)[1];
}

bool commsTx(struct CommsInfo *info, bool bcast, const void *packet, uint32_t len)
{
	uint8_t nonce[AES_CCM_NONCE_SIZE] = {};
	struct MacFrameNormal *mfn;
	struct MacFrameBcast *mfb;
	uint32_t hdrSz;
	char *payload;
	static const struct MacFcs normalFcs = {
		.frameType = FRAME_TYPE_DATA,
		.panIdCompressed = 1,
		.destAddrType = ADDR_MODE_LONG,
		.srcAddrType = ADDR_MODE_LONG,
	};
	static const struct MacFcs broadcastFcs = {
		.frameType = FRAME_TYPE_DATA,
		.destAddrType = ADDR_MODE_SHORT,
		.srcAddrType = ADDR_MODE_LONG,
	};
	
	if (len > COMMS_MAX_PACKET_SZ)
		return false;
	
	if (bcast) {
		mfb = (struct MacFrameBcast*)mCommsBuf.packet;
		hdrSz = sizeof(struct MacFrameBcast);
		payload = (char*)(mfb + 1);
		mfb->fcs = broadcastFcs;
		mfb->seq = mSeq++;
		mfb->dstPan = 0xffff;
		mfb->dstAddr = 0xffff;
		mfb->srcPan = PROTO_PAN_ID;
		macCopy(mfb->src, info->myMac);
	}
	else {
		mfn = (struct MacFrameNormal*)mCommsBuf.packet;
		hdrSz = sizeof(struct MacFrameNormal);
		payload = (char*)(mfn + 1);
		mfn->fcs = normalFcs;
		mfn->seq = mSeq++;
		mfn->pan = PROTO_PAN_ID;
		macCopy(mfn->dst, info->masterMac);
		macCopy(mfn->src, info->myMac);
	}
	
	*(uint32_t*)nonce = (*info->nextIV)++;
	macCopy(nonce + sizeof(uint32_t), info->myMac);
	memcpy(payload, packet, len);
	
	aesCcmEnc(mCommsBuf.packet, mCommsBuf.packet, hdrSz, len, info->encrKey, nonce);
	*(uint32_t*)(payload + len + AES_CCM_MIC_SIZE) = *(uint32_t*)nonce;	//send nonce
	
	len += hdrSz;
	len += AES_CCM_MIC_SIZE;
	len += sizeof(uint32_t);
	
	mCommsBuf.txReq.val0x40 = 0x40;
	mCommsBuf.txReq.txLen = len;
	
	return !radioTx(&mCommsBuf.txReq);
}

int32_t __attribute__((noinline)) commsRx(struct CommsInfo *info, void *data, uint8_t *fromMacP)
{
	uint8_t *buf = mCommsBuf.packet, nonce[13] = {}, fromMac[8];
	int32_t ret = COMMS_RX_ERR_INVALID_PACKET;
	uint32_t len, minNeedLen, hdrLen = 0;
	struct MacFrameFromMaster *mfm;
	struct MacFrameNormal *mfn;
	struct RxBuffer *rb;
	struct MacFcs fcs;
	
	//sort out how many bytes minimum are a valid packet
	minNeedLen = sizeof(struct MacFrameFromMaster);	//mac header
	minNeedLen += sizeof(uint8_t);					//packet type
	minNeedLen += AES_CCM_MIC_SIZE;					//MIC
	minNeedLen += sizeof(uint32_t);					//nonce counter
	minNeedLen += 2 * sizeof(uint8_t);				//RSSI/LQI
	
	rb = radioRxGetNextRxedPacket();
	if (!rb)
		return COMMS_RX_ERR_NO_PACKETS;
	
	//some basic checks
	mfm = (struct MacFrameFromMaster*)rb->data;
	if (rb->len >= sizeof(mCommsBuf.packet) || rb->len < minNeedLen || mfm->fcs.frameType != FRAME_TYPE_DATA ||
			mfm->fcs.secure || mfm->fcs.frameVer || mfm->fcs.destAddrType != ADDR_MODE_LONG || !mfm->fcs.panIdCompressed ||
			(mfm->fcs.srcAddrType != ADDR_MODE_LONG && mfm->fcs.srcAddrType != ADDR_MODE_SHORT) ||
			mfm->pan != PROTO_PAN_ID || !macIsEq(mfm->dst, info->myMac)) {
		radioRxReleaseBuffer(rb);
		return COMMS_RX_ERR_INVALID_PACKET;
	}
	
	//copy out and release buffer
	memcpy(buf, rb->data, len = rb->len - 2 * sizeof(uint8_t));
	mLastLqi = rb->data[len + 0];
	mLastRSSI = rb->data[len + 1];
	
	mfm = (struct MacFrameFromMaster*)buf;
	mfn = (struct MacFrameNormal*)buf;
	radioRxReleaseBuffer(rb);
	
	//sort out header len, copy mac into nonce
	if (mfm->fcs.srcAddrType == ADDR_MODE_LONG) {
		
		macCopy(fromMac, mfn->src);
		hdrLen = sizeof(struct MacFrameNormal);
		
		//re-verify needed length
		minNeedLen -= sizeof(struct MacFrameFromMaster);
		minNeedLen += sizeof(struct MacFrameNormal);
		
		if (rb->len < minNeedLen)
			return COMMS_RX_ERR_INVALID_PACKET;
	}
	else if (mfm->fcs.srcAddrType == ADDR_MODE_SHORT) {
		
		macCopy(fromMac, info->masterMac);
		hdrLen = sizeof(struct MacFrameFromMaster);
	}
	
	//sort out the nonce
	macCopy(nonce + sizeof(uint32_t), fromMac);
	*(uint32_t*)nonce = *(uint32_t*)(buf + len - sizeof(uint32_t));
	
	//decrypt and auth
	len -= hdrLen + AES_CCM_MIC_SIZE + sizeof(uint32_t);
	
	if (!aesCcmDec(buf, buf, hdrLen, len, info->encrKey, nonce))
		return COMMS_RX_ERR_MIC_FAIL;
	
	if (fromMacP)
		macCopy(fromMacP, fromMac);
	
	memcpy(data, buf + hdrLen, len);
	
	return len;
}
