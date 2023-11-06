#include "nrf52840_bitfields.h"
#include "timebase.h"
#include "nrf52840.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "printf.h"
#include "radio.h"

//device expects to see and will send us PAN-ID-compressed packets, PAN is 0x1234 hardcoded. Long addrs used always
//all deveices share one short addr: 0x10AF
//if we advertise to the short addr, device WILL connect to us via long. perhaps worth doing for auto-enrollment


#define MAX_RX_PKTS						8
#define NUM_RETRANSMITS					3
#define ACK_WAIT_MSEC					20


static volatile uint8_t mRxBufs[MAX_RX_PKTS][RADIO_MAX_PACKET_LEN + 1 /* length */ + 1 /* LQI */ + 1/* RSSI */];
static volatile uint8_t mRxNextWrite, mRxNextRead, mRxNumFree, mRxNumGot;
static uint8_t mRxFilterLongMac[8], mTxLongMac[8];
static uint32_t mRxFilterShortMac, mTxShortMac;
static bool mRxEnabled, mPromisc, mAutoAck;
static uint16_t mRxFilterPan, mTxPan;
static volatile int16_t mLastAck;

struct MacHeaderGenericAddr {
	struct MacFcs fixed;
	uint8_t seq;
} __attribute__((packed));

struct MacHeaderShortAddr {
	struct MacFcs fixed;
	uint8_t seq;
	uint16_t pan;
	uint16_t shortDstAddr;
} __attribute__((packed));

struct MacHeaderLongAddr {
	struct MacFcs fixed;
	uint8_t seq;
	uint16_t pan;
	uint8_t longDstAddr[8];
} __attribute__((packed));



void radioTxConfigure(const uint8_t *myLongMac, uint32_t myShortMac, uint16_t pan)
{
	memcpy(mTxLongMac, myLongMac, sizeof(mTxLongMac));
	mTxShortMac = myShortMac;
	mTxPan = pan;
}

static void radioPrvDisable(void)
{
	NRF_RADIO->INTENCLR = 0xffffffff;	//all ints off
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_DISABLE = 1;
	while(!NRF_RADIO->EVENTS_DISABLED);
}

static void radioPrvDoTx(const void* pkt)
{
	radioPrvDisable();
	
	NRF_RADIO->PACKETPTR = (uintptr_t)pkt;
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->TASKS_TXEN = 1;
	while(!NRF_RADIO->EVENTS_DISABLED);
	NRF_RADIO->EVENTS_DISABLED = 0;
	NRF_RADIO->EVENTS_END = 0;
	(void)NRF_RADIO->EVENTS_END;
	NVIC_ClearPendingIRQ(RADIO_IRQn);
}

static void radioPrvQueueUpRx(void)
{
	if (mRxNumFree) {
		
		NRF_RADIO->PACKETPTR = (uintptr_t)mRxBufs[mRxNextWrite];
		NRF_RADIO->EVENTS_END = 0;
		(void)NRF_RADIO->EVENTS_END;
		NRF_RADIO->EVENTS_PAYLOAD = 0;
		(void)NRF_RADIO->EVENTS_PAYLOAD;	//read to avoid re-triggering of irq
		NRF_RADIO->INTENSET = RADIO_INTENSET_PAYLOAD_Msk;
		NRF_RADIO->EVENTS_RXREADY = 0;
		NRF_RADIO->EVENTS_RSSIEND = 0;
		NRF_RADIO->TASKS_RXEN = 1;
	}
	else
		NRF_RADIO->INTENCLR = RADIO_INTENSET_PAYLOAD_Msk;
}

static bool radioPrvMacsEqual(const uint8_t *macA, const uint8_t *macB)
{
	const uint32_t *a = (const uint32_t*)(const char*)macA;
	const uint32_t *b = (const uint32_t*)(const char*)macB;
	
	return a[0] == b[0] && a[1] == b[1];
}

void radioRxAckReset(void)
{
	mLastAck = -1;
}

int16_t radioRxAckGetLast(void)
{
	return mLastAck;
}

void __attribute__((used)) RADIO_IRQHandler(void)
{
	uint8_t *rxedPkt = (uint8_t*)mRxBufs[mRxNextWrite];
	int32_t len = (uint32_t)rxedPkt[0], lenNoCrc = len - sizeof(uint16_t), lenNoMacFixed = lenNoCrc - sizeof(struct MacFcs) - sizeof(uint8_t);
	const struct MacHeaderShortAddr *withShortDst = (const struct MacHeaderShortAddr*)(rxedPkt + 1);
	const struct MacHeaderGenericAddr *generic = (const struct MacHeaderGenericAddr*)(rxedPkt + 1);
	const struct MacHeaderLongAddr *withLongDst = (const struct MacHeaderLongAddr*)(rxedPkt + 1);
	bool crcOk, acceptPacket, sendAck = false;
	uint64_t packetEndTime;
	
	
	//pr("crcOk = %d, lenNoMacFixed = %d, frame %d DadrMode %d\n", crcOk, lenNoMacFixed, generic->fixed.frameType, generic->fixed.destAddrType);
	
	while (!NRF_RADIO->EVENTS_CRCOK && !NRF_RADIO->EVENTS_CRCERROR);
	packetEndTime = timebaseGet();
	
	crcOk = (NRF_RADIO->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) == RADIO_CRCSTATUS_CRCSTATUS_CRCOk; 
	rxedPkt[len] = NRF_RADIO->RSSISAMPLE;
	
	//promiscuous mode accepts anything
	if (mPromisc)
		acceptPacket = true;
	//otherwise, we need a valid crc
	else if (!crcOk)
		acceptPacket = false;
	//packet should be big enough to contain a header
	else if (lenNoMacFixed < 0)
		acceptPacket = false;
	else switch (generic->fixed.frameType) {
		
		case FRAME_TYPE_ACK:
			mLastAck = (uint16_t)generic->seq;
			acceptPacket = false;	//no need to save it as a packet
			break;
		
		case FRAME_TYPE_DATA:	//we are not the coordinator, so we demand to see our address as destination...
			
			switch (generic->fixed.destAddrType) {
				case ADDR_MODE_SHORT:
					acceptPacket = (withShortDst->pan == 0xffff && withShortDst->shortDstAddr == 0xffff) || 
									(withShortDst->pan == mRxFilterPan && (
										withShortDst->shortDstAddr == 0xffff || ((uint32_t)withShortDst->shortDstAddr) == mRxFilterShortMac
									));
					break;
				
				case ADDR_MODE_LONG:
					acceptPacket = withLongDst->pan == mRxFilterPan && radioPrvMacsEqual(withLongDst->longDstAddr, mRxFilterLongMac);
					break;
				
				default:
					acceptPacket = false;
					break;
			}
			sendAck = generic->fixed.ackReqd && mAutoAck;
			break;
		
		default:
			//no riff-raff please
			acceptPacket = false;
			break;
	}
	
	if (acceptPacket) {	//other checks here too plz
	
		if (sendAck) {
			
			static struct {
				uint8_t len;
				struct MacFcs mac;
				uint8_t seq;
			} __attribute__((packed)) ack = {
				.len = sizeof(ack) + sizeof(uint16_t) - sizeof(ack.len),
				.mac = {
					.frameType = FRAME_TYPE_ACK,
				},
			};
			
			ack.seq = generic->seq;
			
			#if 1
				
				//radio ramp up is 40 usec, we need a delay of 120, so we get 80 here
				while (timebaseGet() - packetEndTime < 80 * TIMER_TICKS_PER_SECOND / 1000000);
				
				NRF_RADIO->PACKETPTR = (uintptr_t)&ack;
				NRF_RADIO->EVENTS_DISABLED = 0;
				NRF_RADIO->TASKS_TXEN = 1;
				while(!NRF_RADIO->EVENTS_DISABLED);
				
				
			#else
				
				radioPrvDoTx(&ack);
			#endif
		}
		
		mRxNumFree--;
		if (++mRxNextWrite == MAX_RX_PKTS)
			mRxNextWrite = 0;
		mRxNumGot++;
	}

	NRF_RADIO->EVENTS_END = 0;
	(void)NRF_RADIO->EVENTS_END;
	NRF_RADIO->EVENTS_CRCOK = 0;
	NRF_RADIO->EVENTS_CRCERROR = 0;
	NRF_RADIO->EVENTS_PAYLOAD = 0;
	(void)NRF_RADIO->EVENTS_PAYLOAD;	//read to avoid re-triggering of irq

	NVIC_ClearPendingIRQ(RADIO_IRQn);
	
	radioPrvQueueUpRx();
}

static bool radioPrvRxEnable(void)	//knows that radio is in disabled state
{
	radioPrvQueueUpRx();
	return true;
}

bool radioInit(void)
{
	uint32_t i;
	
	//reset it
	for (i = 0; i < 1000; i++) {
		NRF_RADIO->POWER = 0;
		(void)NRF_RADIO->POWER;
	}
	
	NRF_RADIO->POWER = 1;
	
	radioPrvDisable();
	
	//configure the CRC subunit
	NRF_RADIO->CRCCNF = (RADIO_CRCCNF_SKIPADDR_Ieee802154 << RADIO_CRCCNF_SKIPADDR_Pos) | (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos);
	NRF_RADIO->CRCPOLY = 0x11021;
	NRF_RADIO->CRCINIT = 0;

	//IEE802.15.4 radio settings
	NRF_RADIO->SFD = 0xA7 << RADIO_SFD_SFD_Pos;
	NRF_RADIO->PCNF0 = (8 << RADIO_PCNF0_LFLEN_Pos) | (RADIO_PCNF0_PLEN_32bitZero << RADIO_PCNF0_PLEN_Pos) | RADIO_PCNF0_CRCINC_Msk;
	NRF_RADIO->PCNF1 = ((RADIO_MAX_PACKET_LEN + 1 /* len is part of txed val */) << RADIO_PCNF1_MAXLEN_Pos);
	NRF_RADIO->MODECNF0 = (RADIO_MODECNF0_DTX_Center << RADIO_MODECNF0_DTX_Pos) | (RADIO_MODECNF0_RU_Fast << RADIO_MODECNF0_RU_Pos);
	NRF_RADIO->TIFS = 192;
	
	//general radio things
	NRF_RADIO->TXPOWER = RADIO_TXPOWER_TXPOWER_Pos8dBm << RADIO_TXPOWER_TXPOWER_Pos;
	NRF_RADIO->MODE = RADIO_MODE_MODE_Ieee802154_250Kbit << RADIO_MODE_MODE_Pos;
	
	//shortcuts
	NRF_RADIO->SHORTS = RADIO_SHORTS_PHYEND_DISABLE_Msk | RADIO_SHORTS_READY_START_Msk | RADIO_SHORTS_ADDRESS_RSSISTART_Msk;
	
	mRxEnabled = false;
	radioRxFlush();
	
	NVIC_SetPriority(RADIO_IRQn, 3);
	NVIC_ClearPendingIRQ(RADIO_IRQn);
	NVIC_EnableIRQ(RADIO_IRQn);
	
	return radioSetChannel(RADIO_FIRST_VALID_CHANNEL);
}

bool radioSetChannel(uint_fast8_t channel)
{
	if (channel < RADIO_FIRST_VALID_CHANNEL || channel > RADIO_LAST_VALID_CHANNEL)
		return false;
	
	NRF_RADIO->FREQUENCY = 5 * (channel - 10);
	
	return true;
}

void radioRxFilterCfg(const uint8_t *myMac, uint32_t myShortMac, uint16_t myPan, bool promisc)
{
	mPromisc = promisc;
	mRxFilterShortMac = myShortMac;
	mRxFilterPan = myPan;
	memcpy(mRxFilterLongMac, myMac, sizeof(mRxFilterLongMac));
}

bool radioRxEnable(bool on, bool autoAck)
{
	if (!on == !mRxEnabled)	//is there a state change?
		return true;
	
	mRxEnabled = on;
	mAutoAck = autoAck;
	if (on)
		radioPrvRxEnable();
	else
		radioPrvDisable();
	
	return true;
}

void radioRxFlush(void)
{
	NVIC_DisableIRQ(RADIO_IRQn);
	mRxNextWrite = 0;
	mRxNextRead = 0;
	mRxNumFree = MAX_RX_PKTS;
	mRxNumGot = 0;
	NVIC_EnableIRQ(RADIO_IRQn);
}

int32_t radioRxDequeuePkt(void* dstBuf, uint32_t maxLen, int8_t *rssiP, uint8_t *lqiP)
{
	uint32_t len, copyLen = maxLen;
	
	if (!mRxNumGot)
		return -1;
	
	len = mRxBufs[mRxNextRead][0];
	if (len >= sizeof(uint16_t))	//remove CRC
		len -= sizeof(uint16_t);
	
	if (copyLen > len)
		copyLen = len;
	memcpy(dstBuf, (const void*)(mRxBufs[mRxNextRead] + 1), copyLen);
	if (lqiP) {
		
		uint32_t lqi = mRxBufs[mRxNextRead][len + 1];
		lqi *= 4;
		if (lqi > 255)
			lqi = 255;
		*lqiP = lqi;
	}
	
	if (rssiP)
		*rssiP = -(int8_t)mRxBufs[mRxNextRead][len + 2];
	
	if (++mRxNextRead == MAX_RX_PKTS)
		mRxNextRead = 0;
	
	NVIC_DisableIRQ(RADIO_IRQn);
	mRxNumFree++;
	mRxNumGot--;
	NVIC_EnableIRQ(RADIO_IRQn);
	
	//we freed up a buffer. if RX machine was stalled, resume it
	if (mRxEnabled && !(NRF_RADIO->INTENSET & RADIO_INTENSET_PAYLOAD_Msk))
		radioPrvQueueUpRx();
	
	return len;
}

bool radioTxLL(const void* pkt)
{
	radioPrvDoTx(pkt);
		
	//resume RX if needed
	if (mRxEnabled)
		return radioPrvRxEnable();
	else
		return true;
}



