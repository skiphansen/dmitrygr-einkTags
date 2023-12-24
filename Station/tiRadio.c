#include "nrf52840_bitfields.h"
#include "timebase.h"
#include "nrf52840.h"
#include "tiRadio.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "printf.h"
#include "radio.h"

/*
WIRING:
	P0.15 = nCS
	P0.17 = GDO_0
	P0.20 = GDO_2
	P0.22 = MISO
	P0.24 = CLK
	P1.00 = MOSI
	
	6.5MHz max freq if we do not want to think about anything else
	
	//we configure GDO_2 is for TX.has_fifo_space
	//we configure GDO_0 is for RX.has_data
*/

#define CMD_SRES		0x30
#define CMD_SFSTXON		0x31
#define CMD_SXOFF		0x32
#define CMD_SCAL		0x33
#define CMD_SRX			0x34
#define CMD_STX			0x35
#define CMD_SIDLE		0x36
#define CMD_SWOR		0x38
#define CMD_SPWD		0x39
#define CMD_SFRX		0x3a
#define CMD_SFTX		0x3b
#define CMD_SWORRST		0x3c
#define CMD_SNOP		0x3d

#define REG_IOCFG2		0x00
#define REG_IOCFG1		0x01
#define REG_IOCFG0		0x02
#define REG_FIFOTHR		0x03
#define REG_SYNC1		0x04
#define REG_SYNC0		0x05
#define REG_PKTLEN		0x06
#define REG_PKTCTRL1	0x07
#define REG_PKTCTRL0	0x08
#define REG_ADDR		0x09
#define REG_CHANNR		0x0a
#define REG_FSCTRL1		0x0b
#define REG_FSCTRL0		0x0c
#define REG_FREQ2		0x0d
#define REG_FREQ1		0x0e
#define REG_FREQ0		0x0f
#define REG_MDMCFG4		0x10
#define REG_MDMCFG3		0x11
#define REG_MDMCFG2		0x12
#define REG_MDMCFG1		0x13
#define REG_MDMCFG0		0x14
#define REG_DEVIATN		0x15
#define REG_MCSM2		0x16
#define REG_MCSM1		0x17
#define REG_MCSM0		0x18
#define REG_FOCCFG		0x19
#define REG_BSCFG		0x1a
#define REG_AGCTRL2		0x1b
#define REG_AGCTRL1		0x1c
#define REG_AGCTRL0		0x1d
#define REG_WOREVT1		0x1e
#define REG_WOREVT0		0x1f
#define REG_WORCTRL		0x20
#define REG_FREND1		0x21
#define REG_FREND0		0x22
#define REG_FSCAL3		0x23
#define REG_FSCAL2		0x24
#define REG_FSCAL1		0x25
#define REG_FSCAL0		0x26
#define REG_RCCTRL1		0x27
#define REG_RCCTRL0		0x28
#define REG_FSTEST		0x29
#define REG_PTEST		0x2a
#define REG_AGCTEST		0x2b
#define REG_TEST2		0x2c
#define REG_TEST1		0x2d
#define REG_TEST0		0x2e

#define REG_PATABLE		0x3e
#define REG_FIFO		0x3f

#define REG_PARTNUM		0xf0
#define REG_VERSION		0xf1
#define REG_FREQEST		0xf2
#define REG_LQI			0xf3
#define REG_RSSI		0xf4
#define REG_MARCSTATE	0xf5
#define REG_WORTIME1	0xf6
#define REG_WORTIME0	0xf7
#define REG_PKTSTATUS	0xf8
#define REG_VCO_VC_DAC	0xf9
#define REG_TXBYTES		0xfa
#define REG_RXBYTES		0xfb
#define REG_RCCTRL1_STA	0xfc
#define REG_RCCTRL0_STA	0xfd



#define MAX_RX_PKTS						8
#define NUM_RETRANSMITS					3
#define ACK_WAIT_MSEC					20

static volatile uint8_t mRxBufs[MAX_RX_PKTS][RADIO_MAX_PACKET_LEN + 1 /* length */ + 2 /* RSSI, LQI/STA */];
static volatile uint8_t mRxNextWrite, mRxNextRead, mRxNumFree, mRxNumGot;
static uint8_t mRxFilterLongMac[8], mTxLongMac[8];
static uint32_t mRxFilterShortMac, mTxShortMac;
static bool mRxEnabled, mAutoAck, mPromisc;
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



void tiRadioTxConfigure(const uint8_t *myLongMac, uint32_t myShortMac, uint16_t pan)
{
	memcpy(mTxLongMac, myLongMac, sizeof(mTxLongMac));
	mTxShortMac = myShortMac;
	mTxPan = pan;
}

void tiRadioRxFilterCfg(const uint8_t *myMac, uint32_t myShortMac, uint16_t myPan, bool promisc)
{
	mPromisc = promisc;
	mRxFilterShortMac = myShortMac;
	mRxFilterPan = myPan;
	memcpy(mRxFilterLongMac, myMac, sizeof(mRxFilterLongMac));
}

static bool tiRadioPrvSelect(void)
{
	uint32_t iters = 1024;
	uint64_t time;
	
	NRF_P0->OUTCLR = 1 << 15;
	while (--iters) {
		if (!(NRF_P0->IN & (1 << 22)))
			return true;
	}
	
	//it is taking a while, use precise time
	time = timebaseGet();
	while (timebaseGet() - time < TIMER_TICKS_PER_SECOND * 200 / 1000000) {	//200 usec
		if (!(NRF_P0->IN & (1 << 22)))
			return true;
	}
	
	return false;
}

static void tiRadioPrvDeselect(void)
{
	asm volatile("nop \n nop \n nop \n");
	NRF_P0->OUTSET = 1 << 15;
	asm volatile("nop \n nop \n nop \n");
}

static void tiRadioPrvTxRx(const uint8_t *src, uint_fast8_t txLen, uint8_t *dst, uint_fast8_t rxLen)
{
	asm volatile("":::"memory");
	NRF_SPIM0->RXD.PTR = (uintptr_t)dst;
	NRF_SPIM0->RXD.MAXCNT = rxLen;
	NRF_SPIM0->TXD.PTR = (uintptr_t)src;
	NRF_SPIM0->TXD.MAXCNT = txLen;
	asm volatile("":::"memory");
	NRF_SPIM0->EVENTS_END = 0;
	NRF_SPIM0->TASKS_START = 1;
	while (!NRF_SPIM0->EVENTS_END);
	asm volatile("":::"memory");
}

static int_fast16_t tiRadioPrvStrobe(uint8_t cmd)	//negative on error
{
	if (!tiRadioPrvSelect())
		return -1;
	
	tiRadioPrvTxRx(&cmd, 1, &cmd, 1);
	
	tiRadioPrvDeselect();
	
	return (uint_fast16_t)cmd;
}

static bool tiRadioPrvRegWrite(uint_fast8_t reg, uint_fast8_t val)
{
	uint8_t bytes[] = {reg, val};
	
	if (!tiRadioPrvSelect())
		return false;
	
	tiRadioPrvTxRx(bytes, sizeof(bytes), bytes, sizeof(bytes));
	
	tiRadioPrvDeselect();
	
	return true;
}

static int_fast16_t tiRadioPrvRegRead(uint_fast8_t reg)
{
	uint8_t bytes[] = {reg | 0x80, 0};
	
	if (!tiRadioPrvSelect())
		return -1;
	
	tiRadioPrvTxRx(bytes, sizeof(bytes), bytes, sizeof(bytes));
	
	tiRadioPrvDeselect();
	
	return (uint_fast16_t)bytes[1];
}

static bool tiRadioPrvRegWriteLong(uint8_t reg, const uint8_t *valP, uint8_t len)
{
	if (!tiRadioPrvSelect())
		return false;
	
	reg |= 0x40;	//burst
	tiRadioPrvTxRx(&reg, 1, &reg, 1);
	
	tiRadioPrvTxRx(valP, len, NULL, 0);
	
	tiRadioPrvDeselect();
	
	return true;
}

bool tiRadioRxEnable(bool on, bool autoAck)
{
	bool ret = false;
		
	if (on) {
	
		mAutoAck = autoAck;
		if (mRxEnabled) {
			ret = true;
			goto out;
		}
		
		if (!tiRadioPrvStrobe(CMD_SRX))
			goto out;
		
		mRxEnabled = true;
	}
	else if (mRxEnabled) {
		
		if (!tiRadioPrvStrobe(CMD_SIDLE))
			goto out;
		
		mRxEnabled = false;
	}
	ret = true;
	
out:
	return ret;
}

static bool radioPrvMacsEqual(const uint8_t *macA, const uint8_t *macB)
{
	const uint32_t *a = (const uint32_t*)(const char*)macA;
	const uint32_t *b = (const uint32_t*)(const char*)macB;
	
	return a[0] == b[0] && a[1] == b[1];
}

static uint_fast8_t tiRadioPrvGetState(void)
{
	uint_fast8_t state;
	
	do{
		state = tiRadioPrvRegRead(REG_MARCSTATE);
	} while (tiRadioPrvRegRead(REG_MARCSTATE) != state);
	
	return state;
}

static void tiRadioPrvPacketRx(void)
{
	uint8_t *rxedPkt = (uint8_t*)mRxBufs[mRxNextWrite];
	const struct MacHeaderShortAddr *withShortDst = (const struct MacHeaderShortAddr*)(rxedPkt + 1);
	const struct MacHeaderGenericAddr *generic = (const struct MacHeaderGenericAddr*)(rxedPkt + 1);
	const struct MacHeaderLongAddr *withLongDst = (const struct MacHeaderLongAddr*)(rxedPkt + 1);
	bool crcOk, acceptPacket, sendAck = false;
	int32_t t, lenNoCrc, lenNoMacFixed;
	uint32_t nWaitCycles = 10000;
	uint_fast8_t spiLen, now;
	
	t = tiRadioPrvRegRead(REG_FIFO);
	if (t < 0)
		goto fail;
	
	if (!mRxNumFree)
		goto fail;
	
	spiLen = t;
	if (spiLen > RADIO_MAX_PACKET_LEN)
		goto fail;
	
	t = 0;
	rxedPkt[t++] = lenNoCrc = spiLen;
	now = 31;		//we just read one so 31 left for sure in the FIFO
	spiLen += 2;	//we expect 2 more bytes
	
	while (spiLen) {
		
		uint8_t reg;
		
		if (!tiRadioPrvSelect()) {
			tiRadioPrvDeselect();
			goto fail;
		}
	
		reg = 0xc0 | REG_FIFO;	//burst read
		tiRadioPrvTxRx(&reg, 1, &reg, 1);
		now = reg & 0x0f;
	
		if (now > spiLen)
			now = spiLen;
		
		if (!now && !--nWaitCycles) {
			tiRadioPrvDeselect();
			pr(" !!! RX timeout !!! \n");
			goto fail;
		}
		
		tiRadioPrvTxRx(NULL, 0, rxedPkt + t, now);
		t += now;
		spiLen -= now;
		
		tiRadioPrvDeselect();
	}
	
	rxedPkt++;	//skip len;
	crcOk = !!(rxedPkt[lenNoCrc + 1] & 0x80);
	
	lenNoMacFixed = lenNoCrc - sizeof(struct MacFcs) - sizeof(uint8_t);

	if (0) {
		uint8_t rssiProvided = rxedPkt[lenNoCrc];
		int rssiCalced;
		
		if (rssiProvided >= 128)
			rssiCalced = (rssiProvided - 256) / 2 - 77;
		else
			rssiCalced = rssiProvided / 2 - 77;
		
		pr("RXed packet len %u RSSI %d LQI raw %u CRC: %s", (unsigned)lenNoCrc, rssiCalced, rxedPkt[lenNoCrc + 1] & 0x7f, (crcOk ? "OK" : "BAD"));
		for (t = 0; t < lenNoCrc; t++) {
			if (!(t & 15))
				pr("\n%04x", (unsigned)t);
			pr(" %02x", rxedPkt[t]);
		}
		pr("\n");
	}
	
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
			
			tiRadioTxLL(&ack);
		}
		
		mRxNumFree--;
		if (++mRxNextWrite == MAX_RX_PKTS)
			mRxNextWrite = 0;
		mRxNumGot++;
	}

out:
	if (mRxEnabled && !sendAck)	{//if ack is being TXed, TX irq will restart rx later
		uint32_t maxWait = 100000;
		uint8_t state;
		
		(void)tiRadioPrvStrobe(CMD_SRX);
		
		do {
			state = tiRadioPrvGetState();
			
			if (!--maxWait) {
				pr("too long wait for rx state. state is %d\n", state);
				break;
			}
			
		} while (state != 13 && state != 14 && state != 15);
	}
	return;
	
fail:
	(void)tiRadioPrvStrobe(CMD_SFRX);
	goto out;
}


void tiRadioRxAckReset(void)
{
	mLastAck = -1;
}

int16_t tiRadioRxAckGetLast(void)
{
	return mLastAck;
}

int32_t tiRadioRxDequeuePkt(void* dstBuf, uint32_t maxLen, int8_t *rssiP, uint8_t *lqiP)
{
	uint32_t len, copyLen = maxLen;
	
	if (!mRxNumGot)
		return -1;
	
	len = mRxBufs[mRxNextRead][0];
	
	if (copyLen > len)
		copyLen = len;
	memcpy(dstBuf, (const void*)(mRxBufs[mRxNextRead] + 1), copyLen);
	
	if (lqiP) {
		
		uint32_t lqi = 296 - ((mRxBufs[mRxNextRead][len + 2] & 0x7f) * 2);
		
		//LQI: lower is better. 48 is very good (no lower value seen), 127 is very bad (max value possible)
		//we want LQI 0..255 so we scale as SATURATE(296 - 2 * val)
		
		if (lqi > 255)
			lqi = 255;
		
		*lqiP = lqi;
	}

	if (rssiP)
		*rssiP = ((int8_t)mRxBufs[mRxNextRead][len + 1]) / 2 - 77;

	if (++mRxNextRead == MAX_RX_PKTS)
		mRxNextRead = 0;
	
	NVIC_DisableIRQ(GPIOTE_IRQn);
	mRxNumFree++;
	mRxNumGot--;
	NVIC_EnableIRQ(GPIOTE_IRQn);
	
	//ti radio never stalls RX state machine so nothing to do even if we just freed up a buffer
	return len;
}

void __attribute__((used)) GPIOTE_IRQHandler(void)
{
	NRF_GPIOTE->EVENTS_IN[0] = 0;
	(void)NRF_GPIOTE->EVENTS_IN[0];
	
	while ((NRF_P0->IN >> 17) & 1)		//while there is data (packets)
		tiRadioPrvPacketRx();
}

static void tiRadioPrvIfInit(void)
{
	//configure pins
	NRF_P0->PIN_CNF[15] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	NRF_P0->PIN_CNF[17] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	NRF_P0->PIN_CNF[20] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	NRF_P0->PIN_CNF[22] = (GPIO_PIN_CNF_DIR_Input << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Connect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Pullup << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	NRF_P0->PIN_CNF[24] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	NRF_P1->PIN_CNF[0] = (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos) | (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) | (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos) | (GPIO_PIN_CNF_DRIVE_S0S1 << GPIO_PIN_CNF_DRIVE_Pos) | (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos);
	
	tiRadioPrvDeselect();
	
	//configure SPIM
	NRF_SPIM0->ENABLE = SPIM_ENABLE_ENABLE_Disabled << SPIM_ENABLE_ENABLE_Pos;
	
	NRF_SPIM0->PSEL.SCK = (0 << SPIM_PSEL_SCK_PORT_Pos) | (24 << SPIM_PSEL_SCK_PIN_Pos) | (SPIM_PSEL_SCK_CONNECT_Connected << SPIM_PSEL_SCK_CONNECT_Pos);
	NRF_SPIM0->PSEL.MOSI = (1 << SPIM_PSEL_MOSI_PORT_Pos) | (0 << SPIM_PSEL_MOSI_PIN_Pos) | (SPIM_PSEL_MOSI_CONNECT_Connected << SPIM_PSEL_MOSI_CONNECT_Pos);
	NRF_SPIM0->PSEL.MISO = (0 << SPIM_PSEL_MISO_PORT_Pos) | (22 << SPIM_PSEL_MISO_PIN_Pos) | (SPIM_PSEL_MISO_CONNECT_Connected << SPIM_PSEL_MISO_CONNECT_Pos);
	NRF_SPIM0->PSEL.CSN = (SPIM_PSEL_CSN_CONNECT_Disconnected << SPIM_PSEL_CSN_CONNECT_Pos);
	NRF_SPIM0->PSELDCX = SPIM_PSELDCX_CONNECT_Disconnected << SPIM_PSELDCX_CONNECT_Pos;
	
	NRF_SPIM0->FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M4 << SPIM_FREQUENCY_FREQUENCY_Pos;
	NRF_SPIM0->CONFIG = (SPIM_CONFIG_CPOL_ActiveHigh << SPIM_CONFIG_CPOL_Pos) | (SPIM_CONFIG_CPHA_Leading << SPIM_CONFIG_CPHA_Pos) | (SPIM_CONFIG_ORDER_MsbFirst << SPIM_CONFIG_ORDER_Pos);
	NRF_SPIM0->IFTIMING.RXDELAY = 0 << SPIM_IFTIMING_RXDELAY_RXDELAY_Pos;
	NRF_SPIM0->IFTIMING.CSNDUR = 2 << SPIM_IFTIMING_CSNDUR_CSNDUR_Pos;		//20 usec as per spec
	NRF_SPIM0->CSNPOL = SPIM_CSNPOL_CSNPOL_LOW << SPIM_CSNPOL_CSNPOL_Pos;
	NRF_SPIM0->ORC = 0 << SPIM_ORC_ORC_Pos;
	
	NRF_SPIM0->ENABLE = SPIM_ENABLE_ENABLE_Enabled << SPIM_ENABLE_ENABLE_Pos;
	
	mRxNumFree = MAX_RX_PKTS;
}

static void tiRadioPrvIrqInit(void)
{
	//configure interrupt on RX
	NRF_GPIOTE->INTENCLR = 1 << 0;
	NRF_GPIOTE->CONFIG[0] = (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos) | (0 << GPIOTE_CONFIG_PORT_Pos) | (17 << GPIOTE_CONFIG_PSEL_Pos) | (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos);
	NRF_GPIOTE->EVENTS_IN[0] = 0;
	NRF_GPIOTE->INTENSET = 1 << 0;
	
	NVIC_SetPriority(GPIOTE_IRQn, 7);	//so we can be interrupted by usb
	NVIC_ClearPendingIRQ(GPIOTE_IRQn);
	NVIC_EnableIRQ(GPIOTE_IRQn);
}

bool tiRadioInit(void)
{
   uint8_t regsCfg[] = {
      0x02,  // 0x00 IOCFG2 GDO2: high when TX FIFO at or above the TX FIFO threshold
      0x2e,  // 0x01 IOCFG1 GDO1: high z
      0x01,  // 0x02 IOCFG0 GDO0: high when RX FIFO at or above the RX FIFO threshold or end of packet
      0x07,  // 0x03 FIFOTHR 
      0xd3,  // 0x04 SYNC1 
      0x91,  // 0x05 SYNC0 
      0x7f,  // 0x06 PKTLEN 
      0x04,  // 0x07 PKTCTRL1 
      0x45,  // 0x08 PKTCTRL0 
      0x22,  // 0x09 ADDR 
      0x00,  // 0x0A CHANNR 
#ifdef OLD_RF_CONFIG
      0x0e,  // 0x0B FSCTRL1 FREQ_IF if 355.5 Khz
#else
      0x0f,  // 0x0B FSCTRL1 FREQ_IF if 381 Khz
#endif
      0x00,  // 0x0C FSCTRL0 
      0x22,  // 0x0D FREQ2 
      0xbb,  // 0x0E FREQ1 
      0x13,  // 0x0F FREQ0 
      0x1d,  // 0x10 MDMCFG4 
      0x3b,  // 0x11 MDMCFG3 
      0x13,  // 0x12 MDMCFG2 
#ifdef OLD_RF_CONFIG
      0x43,  // 0x13 MDMCFG1 FEC disabled
      0xa4,  // 0x14 MDMCFG0 channel spacing 333.251953 kHz
#else
      0x73,  // 0x13 MDMCFG1 FEC enabled
      0xa7,  // 0x14 MDMCFG0 channel spacing 335.632324 kHz
#endif

      0x65,  // 0x15 DEVIATN 
      0x07,  // 0x16 MCSM2 
      0x30,  // 0x17 MCSM1 
// NB: the bits in the MCSM0 register are different in the CC1101 and the CC111x !
// 0x1d: XOSC_FORCE_ON = 1, PIN_CTRL_EN = 0, PO_TIMEOUT = 3, FS_AUTOCAL = 1
      0x1d,  // 0x18 MCSM0 

      0x1e,  // 0x19 FOCCFG 
      0x1c,  // 0x1A BSCFG 
      0xc7,  // 0x1B AGCTRL2 
      0x00,  // 0x1C AGCTRL1 
      0xb0,  // 0x1D AGCTRL0 
      0x87,  // 0x1E WOREVT1 
      0x6b,  // 0x1F WOREVT0 
      0xfb,  // 0x20 WORCTRL 
      0xb6,  // 0x21 FREND1 
      0x10,  // 0x22 FREND0 
      0xea,  // 0x23 FSCAL3 
      0x2a,  // 0x24 FSCAL2 
      0x00,  // 0x25 FSCAL1 
      0x1f,  // 0x26 FSCAL0 
      0x41,  // 0x27 RCCTRL1 
      0x00,  // 0x28 RCCTRL0 
   };
   
   uint8_t paTab[] = {0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0};

// Set the frequency offset for MY board, yours will probably be different !
   regsCfg[REG_FSCTRL0] = 0x26;
	
	tiRadioPrvIfInit();
	
	if (tiRadioPrvRegRead(REG_PARTNUM) != 0x00) {
		pr("partnum is wrong\n");
		return false;
	}
	
	if (tiRadioPrvStrobe(CMD_SRES) < 0) {
		pr("res reply\n");
		return false;
	}
	
	if (tiRadioPrvStrobe(CMD_SIDLE) != 0x0f) {
		pr("idle reply\n");
		return false;
	}
	
	if (!tiRadioPrvRegWriteLong(0, regsCfg, sizeof(regsCfg))) {
		pr("config issue\n");
		return false;
	}
	
	if (!tiRadioPrvRegWriteLong(REG_PATABLE, paTab, sizeof(paTab))) {
		pr("PAtable issue\n");
		return false;
	}
	
	tiRadioPrvIrqInit();
	
	pr("Sub-GHz radio inited\n");
	
	tiRadioRxEnable(true, false);
	pr("rx is on\n");

	
	return true;
}

bool tiRadioSetChannel(uint_fast8_t channel)
{
	channel -= SUB_GHZ_CH_OFST;
	if (channel >= SUB_GHZ_NUM_CHANNELS)
		return false;
	
	return tiRadioPrvRegWrite(REG_CHANNR, channel * 3);
}

bool tiRadioTxLL(const void* pkt)
{
	const uint8_t *data = (const uint8_t*)pkt;
	uint32_t len = 1 + *data, now;
	bool ret = false;

	if (tiRadioPrvStrobe(CMD_SIDLE) < 0)
		goto out;
	
	if (tiRadioPrvStrobe(CMD_SFTX) < 0)
		goto out;
	
	now = (len > 64 ? 64 : len);
	if (!tiRadioPrvRegWriteLong(REG_FIFO, data, now))
		goto out;
	len -= now;
	data += now;

	if (tiRadioPrvStrobe(CMD_STX) < 0)
		goto out;
	
	while (len) {
		
		now = 8;
		if (now > len)
			now = len;
		
		while ((NRF_P0->IN >> 20) & 1);	//wait till there is space
		
		if (!tiRadioPrvRegWriteLong(REG_FIFO, data, now))
			goto out;

		data += now;
		len -= now;
	}
	
	while (tiRadioPrvGetState() != 1);
	
	if (!tiRadioPrvStrobe(mRxEnabled ? CMD_SRX : CMD_SIDLE))
		goto out;
	
	ret = true;
	
out:
	return ret;
}



