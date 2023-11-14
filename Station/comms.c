#include "../firmware/common/proto.h"
#include "timebase.h"
#include <stdint.h>
#include <string.h>
#include "tiRadio.h"
#include "printf.h"
#include "radio.h"
#include "comms.h"
#include "led.h"
#include "ccm.h"


struct MacFrameNormal {
   struct MacFcs fcs;
   uint8_t seq;
   uint16_t pan;
   uint8_t dst[8];
   uint8_t src[8];
} __attribute__((packed));

struct MacFrameFromMaster {      //shorter by avoiding source address
   struct MacFcs fcs;
   uint8_t seq;
   uint16_t pan;
   uint8_t dst[8];
   uint16_t src;
} __attribute__((packed));

const uint32_t mCommsPresharedKey[] = PROTO_PRESHARED_KEY;

static uint32_t mIvCounter = 0;
static uint8_t mCommsBuf[133];
static uint8_t mCommsSeq;
static const uint8_t *mMyMac;

static inline void __attribute__((always_inline)) macCopy(uint8_t *restrict dst, const uint8_t *restrict src)
{
   ((uint32_t*)dst)[0] = ((const uint32_t*)src)[0];
   ((uint32_t*)dst)[1] = ((const uint32_t*)src)[1];
}

static inline bool __attribute__((always_inline)) macIsEq(const uint8_t *restrict dst, const uint8_t *restrict src)
{
   return ((uint32_t*)dst)[0] == ((const uint32_t*)src)[0] && ((uint32_t*)dst)[1] == ((const uint32_t*)src)[1];
}

void commsInit(const uint8_t *myMac, uint32_t ivStart)
{
   mMyMac = myMac;
   mIvCounter = ivStart;
}

bool commsTx(uint8_t radioIdx, const uint8_t *toMac, const void *packet, uint32_t payloadLen, bool useSharedKey, bool withAck, bool includeSourceMac)
{
   struct MacFrameNormal *mfn = (struct MacFrameNormal*)(mCommsBuf + 1);
   struct MacFrameFromMaster *mfm = (struct MacFrameFromMaster*)(mCommsBuf + 1);
   uint8_t *payload = includeSourceMac ? (uint8_t*)(mfn + 1) : (uint8_t*)(mfm + 1), nonce[AES_CCM_NONCE_SIZE] = {};
   uint32_t seq, *nonceCtr = (uint32_t*)(payload + payloadLen + AES_CCM_MIC_SIZE), key[4];
   uint8_t *endPtr = (uint8_t*)(nonceCtr + 1);
   struct MacFcs fcs = {
      .frameType = FRAME_TYPE_DATA,
      .panIdCompressed = 1,
      .destAddrType = ADDR_MODE_LONG,
      .srcAddrType = includeSourceMac ? ADDR_MODE_LONG : ADDR_MODE_SHORT,
      .ackReqd = withAck,
   };
   uint32_t nRetries = withAck ? 3 : 1;
   bool ret = false;
   
   bool (*radioTxF)(const void* pkt);
   void (*radioAckResetF)(void);
   int16_t (*radioAckGetLastF)(void);
   
   
   
   if (payloadLen > COMMS_MAX_PACKET_SZ)
      return false;
   
   ledSet(-1, -1, 255, -1);
   
   //put together the frame
   if (includeSourceMac) {
      
      mfn->fcs = fcs;
      mfn->seq = seq = mCommsSeq++;
      mfn->pan = PROTO_PAN_ID;
      macCopy(mfn->dst, toMac);
      macCopy(mfn->src, mMyMac);
   }
   else {
      mfm->fcs = fcs;
      mfm->seq = seq = mCommsSeq++;
      mfm->pan = PROTO_PAN_ID;
      macCopy(mfm->dst, toMac);
      mfm->src = 0xffff;
   }
   memcpy(payload, packet, payloadLen);
   
   //prepare nonce, store it
   *(uint32_t*)nonce = *nonceCtr = mIvCounter++;
   macCopy(nonce + sizeof(uint32_t), mMyMac);
   
   //encrypt/auth
   if (!useSharedKey)
      commsExtKeyForMac(key, toMac);
   aesCcmEnc(payload, payload, payloadLen, mCommsBuf + 1, payload - (mCommsBuf + 1), useSharedKey ? mCommsPresharedKey : key, nonce);
   
   mCommsBuf[0] = endPtr - &mCommsBuf[1];
   
   
   if (radioIdx == 0) { //nRF radio
      
      mCommsBuf[0] += sizeof(uint16_t) /* fcs is auto-generated but we need to account for its length */;
      radioTxF = radioTxLL;
      radioAckResetF = radioRxAckReset;
      radioAckGetLastF = radioRxAckGetLast;
   }
   else if (radioIdx == 1){   //TI radio
      
      //no len accounting needed
      radioTxF = tiRadioTxLL;
      radioAckResetF = tiRadioRxAckReset;
      radioAckGetLastF = tiRadioRxAckGetLast;
   }
   else
      return false;
   
   do {
      
      uint64_t time;
      
      radioAckResetF();
      
      if (!radioTxF(mCommsBuf))
         break;
      
      if (!withAck) {
         ret = true;
         break;
      }
      
      time = timebaseGet();
      while (timebaseGet() - time < TIMER_TICKS_PER_SECOND * 500 / 1000000) {    //500us wait
         
         if (radioAckGetLastF() == seq) {
            ret = true;
            nRetries = 0;
            break;
         }
      }
      
   } while (nRetries--);
   
   ledSet(-1, -1, 0, -1);
   
   return ret;
}

int32_t commsRx(uint32_t radiosMask, uint8_t *radioIdxP, void *data, uint8_t *fromMac, int8_t *rssiP, uint8_t *lqiP, bool *wasBcastP)
{
   uint32_t expectedMinLen = sizeof(struct MacFcs) + sizeof(uint8_t) /* seq */ + AES_CCM_MIC_SIZE + sizeof(uint32_t) /* nonce */, key[4] = {};
   uint8_t *srcAddr = NULL, *dstAddr = NULL, *start, nonce[AES_CCM_NONCE_SIZE] = {};
   const struct MacFcs *fcs = (const struct MacFcs*)mCommsBuf;
   bool verified, bcast = false;
   uint16_t dstPan, srcPan;
   int32_t ret;
   
   
   ret = (radiosMask & 1) ? radioRxDequeuePkt(mCommsBuf, sizeof(mCommsBuf), rssiP, lqiP) : -1;
   if (ret >= 0)
      *radioIdxP = 0;
   if (ret < 0) {
      ret = (radiosMask & 2) ? tiRadioRxDequeuePkt(mCommsBuf, sizeof(mCommsBuf), rssiP, lqiP) : -1;
      *radioIdxP = 1;
   }
   
   if (ret < 0)
      return COMMS_RX_ERR_NO_PACKETS;
   
   if (ret < expectedMinLen)
      return COMMS_RX_ERR_INVALID_PACKET;
   
   if (fcs->frameType != FRAME_TYPE_DATA || fcs->frameVer || fcs->rfu1 || fcs->secure)
      return COMMS_RX_ERR_INVALID_PACKET;
   
   
   //compressed PAN IDs are only allowed when do HAVE addresses
   if ((fcs->destAddrType == ADDR_MODE_NONE || fcs->srcAddrType == ADDR_MODE_NONE) && fcs->panIdCompressed)
      return COMMS_RX_ERR_INVALID_PACKET;

   start = ((uint8_t*)(fcs + 1)) + 1;  //skip FCS & seq

   switch (fcs->destAddrType) {
      case ADDR_MODE_NONE:
         break;
      
      default:
         return false;
      
      case ADDR_MODE_SHORT:
         dstAddr = start + 2;
         dstPan = *(const uint16_t*)start;
         start += 4;
         expectedMinLen += 4; //PAN and addr
         bcast = (dstPan == 0xffff) || (dstPan == PROTO_PAN_ID && *(const uint16_t*)dstAddr == 0xffff);
         break;
      
      case ADDR_MODE_LONG:
         dstAddr = start + 2;
         dstPan = *(const uint16_t*)start;
         start += 10;
         expectedMinLen += 10;   //PAN and addr
         break;
   }
   
   switch (fcs->srcAddrType) {
      case ADDR_MODE_NONE:
         break;
      
      default:
         return false;
      
      case ADDR_MODE_SHORT:
         if (fcs->panIdCompressed) {
            srcAddr = start;
            srcPan = dstPan;
            start += 2;
            expectedMinLen += 2;
         }
         else {
            srcAddr = start + 2;
            srcPan = *(const uint16_t*)start;
            start += 4;
            expectedMinLen += 4;
         }
         break;
      
      case ADDR_MODE_LONG:
         if (fcs->panIdCompressed) {
            srcAddr = start;
            srcPan = dstPan;
            start += 8;
            expectedMinLen += 8;
         }
         else {
            srcAddr = start + 2;
            srcPan = *(const uint16_t*)start;
            start += 10;
            expectedMinLen += 10;
         }
         break;
   }
   
   //make sure we still have enough length
   if (ret < expectedMinLen)
      return COMMS_RX_ERR_INVALID_PACKET;
   
   //quiet down gcc
   (void)srcPan;

   if (0) {
      
      pr("PACKET FROM ");
      if (!srcAddr)
         pr("<COORDINATOR>");
      else if (fcs->srcAddrType == ADDR_MODE_LONG)
         pr("%04x:" MACFMT, srcPan, MACCVT(srcAddr));
      else
         pr("%04x:%04x", srcPan, *(const uint16_t*)srcAddr);
      pr(" TO ");
      if (!dstAddr)
         pr("<COORDINATOR>");
      else if (dstPan == 0xffff)
         pr("<BROADCAST TO ALL>");
      else if (fcs->destAddrType == ADDR_MODE_LONG)
         pr("%04x:" MACFMT, dstPan, MACCVT(dstAddr));
      else if (*(const uint16_t*)dstAddr == 0xffff)
         pr("%04x:<BROADCAST>", dstPan);
      else
         pr("%04x:%04x", dstPan, *(const uint16_t*)dstAddr);
      pr("\n");
   }
   
   ret -= (start - mCommsBuf);      //subtract size of non-encrypted header from length
   
   *(uint32_t*)nonce = *(uint32_t*)(start + ret - sizeof(uint32_t));
   if (fcs->srcAddrType == ADDR_MODE_LONG)
      macCopy(nonce + sizeof(uint32_t), srcAddr);
   
   //try with a per-device key
   commsExtKeyForMac(key, srcAddr);
   verified = aesCcmDec(data, start, ret - sizeof(uint32_t) /* nonce */ - AES_CCM_MIC_SIZE, mCommsBuf, start - mCommsBuf, key, nonce);
   
   //broadcasts are allowed to be signed using the preshared key
   if (!verified && bcast)
      verified = aesCcmDec(data, start, ret - sizeof(uint32_t) /* nonce */ - AES_CCM_MIC_SIZE, mCommsBuf, start - mCommsBuf, mCommsPresharedKey, nonce);
   
   if (!verified) {
      pr("%ccast crc was valid, mic failed, from " MACFMT "\n", bcast ? 'b' : 'u', MACCVT(srcAddr));
      return COMMS_RX_ERR_MIC_FAIL;
   }
   
   if (fromMac) {
      if (fcs->srcAddrType == ADDR_MODE_LONG)
         macCopy(fromMac, srcAddr);
      else
         memset(fromMac, 0, 8);
   }
   
   if (wasBcastP)
      *wasBcastP = bcast;
   
   ret -= sizeof(uint32_t) /* nonce */ + AES_CCM_MIC_SIZE;
   
   return ret;
}






