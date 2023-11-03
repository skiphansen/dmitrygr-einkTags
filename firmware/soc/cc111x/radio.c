#include "asmUtil.h"
#include "printf.h"
#include "radio.h"
#include "board.h"
#include "cpu.h"


/*
	we use 1-MHz spaced channels 903MHz..927MHz
	250 Kbps, 165KHz deviation, GFSK
	Packets will be variable length, CRC16 will be provided by the radio, as will whitening. 
*/




struct MacHeaderGenericAddr {
	struct MacFcs fcs;
	uint8_t seq;
};

struct MacHeaderShortAddr {
	struct MacFcs fcs;
	uint8_t seq;
	uint16_t pan;
	uint16_t shortDstAddr;
};

struct MacHeaderLongAddr {
	struct MacFcs fcs;
	uint8_t seq;
	uint16_t pan;
	uint8_t longDstAddr[8];
};

static const __code uint8_t mRadioCfg[] = {
	0xd3,	//SYNC1
	0x91,	//SYNC0
	RADIO_MAX_PACKET_LEN,	//PKTLEN
	0x04,	//PKTCTRL1
	0x45,	//PKTCTRL0
	0x22,	//ADDR
	0x00,	//CHANNR
	0x0f,	//FSCTRL1
	0x00,	//FSCTRL0
	0x22,	//FREQ2
	0xbb,	//FREQ1
	0x13,	//FREQ0
	0x1d,	//MDMCFG4
	0x3b,	//MDMCFG3
	0x13,	//MDMCFG2
	0x73,	//MDMCFG1
	0xa7,	//MDMCFG0
	0x65,	//DEVIATN
	0x07,	//MCSM2
	0x00,	//MCSM1 - no cca
	0x18, 	//MCSM0
	0x1e,	//FOCCFG
	0x1c,	//BSCFG
	0xc7,	//AGCCTRL2
	0x00,	//AGCCTRL1
	0xb0,	//AGCCTRL0
	0xb6,	//FREND1
	0x10,	//FREND0
	0xea,	//FSCAL3
	0x2a,	//FSCAL2
	0x00,	//FSCAL1
	0x1f,	//FSCAL0
};

/*
	revelant cc1111 sniffer config:
		FSCTRL1   |0xDF07|0x0C|Frequency Synthesizer Control 
		FREQ2     |0xDF09|0x25|Frequency Control Word, High Byte 
		FREQ1     |0xDF0A|0xA0|Frequency Control Word, Middle Byte 
		FREQ0     |0xDF0B|0x00|Frequency Control Word, Low Byte 
		MDMCFG4   |0xDF0C|0x0D|Modem configuration 
		MDMCFG3   |0xDF0D|0x55|Modem Configuration 
		MDMCFG2   |0xDF0E|0x13|Modem Configuration 
		MDMCFG1   |0xDF0F|0x23|Modem Configuration 
		MDMCFG0   |0xDF10|0xC7|Modem Configuration 
		DEVIATN   |0xDF11|0x66|Modem Deviation Setting 
		MCSM0     |0xDF14|0x18|Main Radio Control State Machine Configuration 
		FOCCFG    |0xDF15|0x1D|Frequency Offset Compensation Configuration 
		BSCFG     |0xDF16|0x1C|Bit Synchronization Configuration 
		AGCCTRL2  |0xDF17|0xC7|AGC Control 
		AGCCTRL1  |0xDF18|0x00|AGC Control 
		AGCCTRL0  |0xDF19|0xB0|AGC Control 
		FREND1    |0xDF1A|0xB6|Front End RX Configuration 
		FSCAL3    |0xDF1C|0xEA|Frequency Synthesizer Calibration 
		FSCAL2    |0xDF1D|0x2A|Frequency Synthesizer Calibration 
		FSCAL1    |0xDF1E|0x00|Frequency Synthesizer Calibration 
		FSCAL0    |0xDF1F|0x1F|Frequency Synthesizer Calibration 
		TEST1     |0xDF24|0x31|Various Test Settings 
		TEST0     |0xDF25|0x09|Various Test Settings 
		PA_TABLE0 |0xDF2E|0x8E|PA Power Setting 0 
		IOCFG0    |0xDF31|0x06|Radio Test Signal Configuration (P1_5) 

*/

#define RX_BUFFER_SIZE			(RADIO_MAX_PACKET_LEN + 1 /* len byte */ + 2 /* RSSI & LQI */)
#define RX_BUFFER_NUM			3

static volatile uint8_t __xdata mLastAckSeq;
static uint8_t __xdata mRxFilterLongMac[8];
static volatile __xdata uint16_t mRxFilterShortMac, mRxFilterPan;
static volatile __bit mRxOn, mHaveLastAck, mRxFilterAllowShortMac, mAutoAck;
static volatile uint8_t __xdata mRxBufs[RX_BUFFER_NUM][RX_BUFFER_SIZE];

#define mRxBufNextR		T2PR
#define mRxBufNextW		T4CC0
#define mRxBufNumFree	T4CC1

static volatile struct DmaDescr __xdata mRadioRxDmaCfg = {
	.srcAddrHi = 0xdf,					//RFD is source
	.srcAddrLo = 0xd9,
	.lenHi = 0,
	.vlen = 4,							//n + 3 bytes transferred (len, and the 2 status bytes)
	.lenLo = RX_BUFFER_SIZE,			//max xfer
	.trig = 19,							//radio triggers
	.tmode = 0,							//single mode
	.wordSize = 0,						//transfer bytes
	.priority = 2,						//higher PRIO than CPU
	.m8 = 0,							//entire 8 bits are valid length (let RADIO limit it)
	.irqmask = 1,						//interrupt on done
	.dstinc = 1,						//increment dst
	.srcinc = 0,						//do not increment src
};
static volatile struct DmaDescr __xdata mRadioTxDmaCfg = {
	.dstAddrHi = 0xdf,					//RFD is destination
	.dstAddrLo = 0xd9,
	.lenHi = 0,
	.vlen = 1,							//n + 1 bytes transferred (len byte itself)
	.lenLo = RADIO_MAX_PACKET_LEN + 1,	//max xfer
	.trig = 19,							//radio triggers
	.tmode = 0,							//single mode
	.wordSize = 0,						//transfer bytes
	.priority = 2,						//higher PRIO than CPU
	.m8 = 0,							//entire 8 bits are valid length (let RADIO limit it)
	.irqmask = 0,						//do not interrupt on done
	.dstinc = 0,						//do not increment dst
	.srcinc = 1,						//increment src
};


void radioSetChannel(uint8_t ch)
{
	ch -= RADIO_FIRST_CHANNEL;
	if (ch < RADIO_NUM_CHANNELS)
		CHANNR = ch * 3;
}

#pragma callee_saves radioPrvGoIdle
static void radioPrvGoIdle(void)
{
	RFST = 4;					//radio, go idle!
	while (MARCSTATE != 1);
}

void radioInit(void)
{
	volatile uint8_t __xdata *radioRegs = (volatile uint8_t __xdata *)0xdf00;
	uint8_t i;
	
	radioPrvGoIdle();
	
	for (i = 0; i < sizeof(mRadioCfg); i++)
		radioRegs[i] = mRadioCfg[i];

	TEST2 = 0x88;
	TEST1 = 0x31;
	TEST0 = 0x09;
	PA_TABLE0 = 0x8E;
	RFIM = 0x10;	//only irq on done
	
	mRxBufNextW = 0;
	mRxBufNextR = 0;
	mRxBufNumFree = RX_BUFFER_NUM;
}

void radioSetTxPower(int8_t dBm)	//-30..+10 dBm
{
	//for 915 mhz, as per https://www.ti.com/lit/an/swra151a/swra151a.pdf
	//this applies to cc1110 as per: https://e2e.ti.com/support/wireless-connectivity/other-wireless/f/667/t/15808
	static const __code uint8_t powTab[] = {
		/* -30 dBm */0x03, /* -28 dBm */0x05, /* -26 dBm */0x06, /* -24 dBm */0x08,
		/* -22 dBm */0x0b, /* -20 dBm */0x0e, /* -18 dBm */0x19, /* -16 dBm */0x1c,
		/* -14 dBm */0x24, /* -12 dBm */0x25, /* -10 dBm */0x27, /*  -8 dBm */0x29,
		/*  -6 dBm */0x38, /*  -4 dBm */0x55, /*  -2 dBm */0x40, /*   0 dBm */0x8d,
		/*   2 dBm */0x8a, /*   4 dBm */0x84, /*   6 dBm */0xca, /*   8 dBm */0xc4,
		/*  10 dBm */0xc0,
	};
	
	if (dBm < -30)
		dBm = -30;
	else if (dBm > 10)
		dBm = 10;
	
	//sdcc calles integer division here is we use " / 2", le sigh...
	PA_TABLE0 = powTab[((uint8_t)(dBm + 30)) >> 1];
}

#pragma callee_saves radioPrvSetDmaCfgAndArm
static void radioPrvSetDmaCfgAndArm(uint16_t cfgAddr)
{
	DMA0CFG = cfgAddr;
	DMAARM = 1;
	__asm__ (" nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n");	//as per spec
}

#pragma callee_saves radioPrvDmaAbort
static void radioPrvDmaAbort(void)
{
	DMAARM = 0x81;
	__asm__ (" nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n nop \n");	//as per spec
}

#pragma callee_saves radioPrvSetupRxDma
static void radioPrvSetupRxDma(uint8_t __xdata* buf)
{
	uint16_t addr = (uint16_t)buf;
	
	radioPrvDmaAbort();
	mRadioRxDmaCfg.dstAddrHi = addr >> 8;
	mRadioRxDmaCfg.dstAddrLo = addr & 0xff;
	radioPrvSetDmaCfgAndArm((uint16_t)(volatile void __xdata*)mRadioRxDmaCfg);
}

#pragma callee_saves radioPrvSetupTxDma
static void radioPrvSetupTxDma(const uint8_t __xdata* buf)
{
	uint16_t addr = (uint16_t)buf;
	
	radioPrvDmaAbort();
	mRadioTxDmaCfg.srcAddrHi = addr >> 8;
	mRadioTxDmaCfg.srcAddrLo = addr & 0xff;
	radioPrvSetDmaCfgAndArm((uint16_t)(volatile void __xdata*)mRadioTxDmaCfg);
}

static void radioPrvRxStartListenIfNeeded(void)
{
	if (!mRxOn || !mRxBufNumFree)
		return;
	
	radioPrvGoIdle();
	
	IRCON &= (uint8_t)~(1 << 0);	//clear dma irq flag
	IEN1 |= 1 << 0;					//dma irq on
	
	radioPrvSetupRxDma(mRxBufs[mRxBufNextW]);
	
	RFST = 2;	//rx
}

#pragma callee_saves radioRxStopIfRunning
static void radioRxStopIfRunning(void)
{
	IEN1 &= (uint8_t)~(1 << 0);			//dma irq off
	radioPrvDmaAbort();
	radioPrvGoIdle();
	
	DMAIRQ = 0;							//clear any lagging DMA irqs
	IRCON &= (uint8_t)~(1 << 0);		//clear global dma irq flag
}

void radioTx(const void __xdata *packet)	//len included please
{
	const uint8_t __xdata *src = (const uint8_t __xdata*)packet;
	
	radioRxStopIfRunning();
	
	radioPrvSetupTxDma(packet);
	RFIF = 0;
	
	RFST = 3;	//TX
	
	while (!(RFIF & (1 << 4)));
	
	radioPrvDmaAbort();						//abort DMA just in case
	
	radioPrvRxStartListenIfNeeded();
}

void DMA_ISR(void) __interrupt (8)
{
	if (DMAIRQ & (1 << 0)) {
		
		#define hdr		((struct MacHeaderGenericAddr __xdata*)(buf + 1))	//SDCC wastes RAM on these as variables, so make them defines
		#define hdrSA	((struct MacHeaderShortAddr __xdata*)(buf + 1))
		#define hdrLA	((struct MacHeaderLongAddr __xdata*)(buf + 1))
		uint8_t __xdata *buf = mRxBufs[mRxBufNextW];
		__bit acceptPacket = false;
		uint8_t len;
		
		
		DMAIRQ &= (uint8_t)~(1 << 0);
		len = buf[0];
			
		//verify length was proper, crc is a match 
		if (len <= RADIO_MAX_PACKET_LEN && len >= sizeof(struct MacHeaderGenericAddr) && (buf[(uint8_t)(len + 2)] & 0x80) &&
			!hdr->fcs.secure && !hdr->fcs.rfu1 && !hdr->fcs.rfu2 && !hdr->fcs.frameVer) {
			
			switch (hdr->fcs.frameType) {
				case FRAME_TYPE_DATA:
					switch (hdr->fcs.destAddrType) {
						case ADDR_MODE_SHORT:
							acceptPacket = (hdrSA->pan == 0xffff && hdrSA->shortDstAddr == 0xffff) || 
												(hdrSA->pan == mRxFilterPan && (
													hdrSA->shortDstAddr == 0xffff || (
														mRxFilterAllowShortMac && hdrSA->shortDstAddr == mRxFilterShortMac
													)
												)
											);
							break;
						
						case ADDR_MODE_LONG:
							acceptPacket = hdrLA->pan == mRxFilterPan && xMemEqual(hdrLA->longDstAddr, mRxFilterLongMac, 8);
							break;
						
						default:
							break;
					}
					break;
					
				case FRAME_TYPE_ACK:
					mLastAckSeq = hdr->seq;
					mHaveLastAck = true;
					//fallthrough
				default:
					break;
			}
		}
		
		if (acceptPacket) {	//other checks here too plz
	
			if (++mRxBufNextW == RX_BUFFER_NUM)
				mRxBufNextW = 0;
			mRxBufNumFree--;
		}

		radioPrvRxStartListenIfNeeded();
	}
	else
		pr("dma irq unexpected\n");
	
	IRCON &= (uint8_t)~(1 << 0);
}

void radioRxFilterCfg(const uint8_t __xdata *filterForLong, uint32_t myShortMac, uint16_t myPan)
{
	xMemCopy8(mRxFilterLongMac, filterForLong);
	
	if (myShortMac >> 16)
		mRxFilterAllowShortMac = false;
	else {
		mRxFilterAllowShortMac = true;
		mRxFilterShortMac = myShortMac;
	}
	mRxFilterPan = myPan;
}

void radioRxAckReset(void)
{
	mHaveLastAck = false;
}

int16_t radioRxAckGetLast(void)
{
	if (mHaveLastAck)
		return (uint16_t)mLastAckSeq;
	else
		return -1;
}

void radioRxEnable(__bit on, __bit autoAck)
{
	if (!on) {
		radioRxStopIfRunning();
		mRxOn = false;
	}
	else if (!mRxOn) {
		
		mRxOn = true;
		radioPrvRxStartListenIfNeeded();
	}
	mAutoAck = autoAck;
}

int8_t radioRxDequeuePktGet(const void __xdata * __xdata *dstBufP, uint8_t __xdata *lqiP, int8_t __xdata *rssiP)
{
	const uint8_t __xdata *buf = mRxBufs[mRxBufNextR];
	uint8_t lqi, len = buf[0];
	
	if (mRxBufNumFree == RX_BUFFER_NUM)
		return -1;
	
	lqi = 148 - (buf[len + 2] & 0x7f);
	if (lqi >= 127)
		lqi = 255;
	else
		lqi *= 2;
	*lqiP = lqi;
	
	*rssiP = ((int8_t)buf[len + 1] >> 1) - 77;
	
	if (dstBufP)
		*dstBufP = buf + 1;
	
	return len;
}

void radioRxDequeuedPktRelease(void)
{
	uint8_t prevNumFree;
	
	if (++mRxBufNextR == RX_BUFFER_NUM)
		mRxBufNextR = 0;
	
	__critical {
		prevNumFree = mRxBufNumFree++;
	}
	
	if (!prevNumFree)
		radioPrvRxStartListenIfNeeded();
}

void radioRxFlush(void)
{
	uint8_t wasFree;
	
	__critical {
		wasFree = mRxBufNumFree;
		mRxBufNumFree = RX_BUFFER_NUM;
	}
	
	if (!wasFree)
		radioPrvRxStartListenIfNeeded();
}
