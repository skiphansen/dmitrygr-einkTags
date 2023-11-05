#include "asmUtil.h"
#include "screen.h"
#include "printf.h"
#include "cc111x.h"
#include "timer.h"
#include "sleep.h"

uint8_t __xdata mScreenVcom;

/*
	//eInk pinout:
	//	pin  1 = MFCSB		= ? optional
	//	pin  8 = BS1		= P0.0
	//	pin  9 = BUSY		= P1.0
	//	pin 10 = nRESET		= P1.2
	//	pin 11 = D/nC		= P0.7
	//	pin 12 = nCS		= P1.1
	//	pin 13 = D0(SCK)	= P1.3
	//	pin 14 = D1(SDIN)	= P1.5 (also can be data out)
	//	pin 19 = FMSDO		= ?	optional
	//extra pins
	//	nEnable				= P0.6
	

	//lut struct similar to https://v4.cecdn.yun300.cn/100001_1909185148/UC8151C-1.pdf
	
	struct LutCommandData {
		struct LutCommandEntry entry[6 or 10];	//10 for vcom and red, 6 for black and white
	}
	
	struct LutCommandEntry {
		uint8_t levelSelects;	//2 bits each, top to bottom
		uint8_t numFrames[4]	//for each frame
		uint8_t numRepeats;
	}
	
	//for vcom lut (0x20), levels are: 00 - VCOM_DC, 01 - VCOM_DC + VDH, 10 - VCOM_DC + VDL, 11 - Float
	//for other LUTs: levels are: 00 - GND, 01 - VDH, 10 - VDL, 11 - VDHR
	
	//0x20 is vcom
	//0x22 is red LUT
	//0x23 is white LUT
	//0x24 is black LUT
*/

__xdata __at (0xfda2) uint8_t mScreenRow[320];	//350 bytes here, we use 320 of them

static __bit mInited = false, mPartial;
static uint8_t __xdata mPassNo;


static const uint8_t __code mLutStage1_Vcom[] = {
	
	0x00, 0x0f, 0x16, 0x1f, 0x3e, 0x01,
	0x00, 0x48,	0x48, 0x00, 0x00, 0x0c,
	0x00, 0x0a, 0x0a, 0x00,	0x00, 0x19,
	0x00, 0x02, 0x03, 0x00, 0x00, 0x19,
	0x00, 0x02,	0x03, 0x00, 0x00, 0x06,
	0x00, 0x2b, 0x00, 0x00, 0x00, 0x09,
	0x00, 0x8e,	0x00, 0x00, 0x00, 0x05,
};

static const uint8_t __code mLutStage1_00[] = {	//black

	0x0a, 0x0f, 0x16, 0x1f, 0x3e, 0x01,
	0x90, 0x48,	0x48, 0x00, 0x00, 0x0c,
	0x90, 0x0a, 0x0a, 0x00,	0x00, 0x19,
	0x10, 0x02, 0x03, 0x00, 0x00, 0x19,
	0x10, 0x02,	0x03, 0x00, 0x00, 0x06,
};

static const uint8_t __code mLutStage1_01[] = {	//white
	
	0x01, 0x0f, 0x16, 0x1f, 0x3e, 0x01,
	0x90, 0x48,	0x48, 0x00, 0x00, 0x0c,
	0x90, 0x0a, 0x0a, 0x00,	0x00, 0x19,
	0x80, 0x02, 0x03, 0x00, 0x00, 0x19,
	0x80, 0x02,	0x03, 0x00, 0x00, 0x06,
};

static const uint8_t __code mLutStage1_10[] = {	//red

	0xaa, 0x0f, 0x16, 0x1f, 0x3e, 0x01,
	0x90, 0x48,	0x48, 0x00, 0x00, 0x0c,
	0x90, 0x0a, 0x0a, 0x00,	0x00, 0x19,
	0x90, 0x02, 0x03, 0x00, 0x00, 0x19,
	0x00, 0x02,	0x03, 0x00, 0x00, 0x06,
	0xb0, 0x03, 0x28, 0x00, 0x00, 0x09,
	0xc0, 0x8e,	0x00, 0x00, 0x00, 0x05,
};


static const uint8_t __code mLutStage2_Vcom[] = {
	
	0x00, 0x10, 0x08, 0x00, 0x00, 0x34,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage2_00[] = {		//this is the no-op "keep" lut here

	0x00, 0x10, 0x08, 0x00, 0x00, 0x34,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage2_01[] = {			//this is the "darken a bit" lut

	0x90, 0x10, 0x04, 0x04, 0x00, 0x34,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage2_10[] = {			//this is the "darken a lot" lut

	0x90, 0x10, 0x07, 0x01, 0x00, 0x34,
	0x00, 0x40, 0x00, 0x00, 0x00, 0x05
};


static const uint8_t __code mLutStage3_Vcom[] = {
	
	0x00, 0x02, 0x04, 0x01, 0x00, 0x02,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage3_00[] = {			//this is the no-op "keep" lut here

	0x00, 0x02, 0x04, 0x01, 0x00, 0x02,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage3_01[] = {			//this is the "re-blacken lut" lut

	0x44, 0x02, 0x04, 0x01, 0x00, 0x02,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage3_10[] = {			//this is the "re-reden lut" lut

	0xc0, 0x8e,	0x00, 0x00, 0x00, 0x02,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage4_Vcom[] = {
	
	0x00, 0x02, 0x04, 0x01, 0x00, 0x01,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage4_00[] = {			//this is the no-op "keep" lut here

	0x00, 0x02, 0x04, 0x01, 0x00, 0x01,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage4_01[] = {			//this is the "re-whiten lut" lut

	0x88, 0x02, 0x04, 0x01, 0x00, 0x01,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutStage4_10[] = {			//this is the no-op "keep" lut here (UNUSED)

	0x00, 0x02, 0x04, 0x01, 0x00, 0x01,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};


static const uint8_t __code mLutPartial_Vcom[] = {
	
	0x00, 0x04, 0x01, 0x04, 0x01, 0x08,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutPartial_00[] = {			//this is the no-op "keep" LUT

	0x00, 0x04, 0x01, 0x04, 0x01, 0x08,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutPartial_01[] = {			//this is the "W2B" LUT

	0x44, 0x04, 0x01, 0x04, 0x01, 0x08,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};

static const uint8_t __code mLutPartial_10[] = {			//this is the "B2W" LUT

	0x88, 0x04, 0x01, 0x04, 0x01, 0x08,
	0x00, 0x10, 0x00, 0x00, 0x00, 0x05
};



#pragma callee_saves screenPrvWaitByteSent
static inline void screenPrvWaitByteSent(void)
{
	while (U0CSR & 0x01);
}

#pragma callee_saves screenPrvSendByte
static void screenPrvSendByte(uint8_t byte)
{
	screenPrvWaitByteSent();
	U0DBUF = byte;
}

#pragma callee_saves screenPrvSendCommand
static inline void screenPrvSendCommand(uint8_t cmdByte)
{
	screenPrvWaitByteSent();
	P0 &= (uint8_t)~(1 << 7);
	__asm__("nop");
	screenPrvSendByte(cmdByte);
	__asm__("nop");
	screenPrvWaitByteSent();
	P0 |= (1 << 7);
}

#pragma callee_saves screenPrvSendData
static inline void screenPrvSendData(uint8_t cmdByte)
{
	screenPrvSendByte(cmdByte);
}

#pragma callee_saves einkSelect
static inline void einkSelect(void)
{
	P1 &= (uint8_t)~(1 << 1);
	__asm__("nop");	//60ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

#pragma callee_saves einkDeselect
static inline void einkDeselect(void)
{
	screenPrvWaitByteSent();
	__asm__("nop");	//20ns between select and anything else as per spec. at our clock speed that is less than a single cycle, so delay a cycle
	P1 |= (uint8_t)(1 << 1);
	__asm__("nop");	//40ns between deselect select and reselect as per spec. at our clock speed that is less than a single cycle, so delay a cycle
}

void P1INT_ISR(void) __interrupt (15)
{
	SLEEP &= (uint8_t)~(3 << 0);	//wake up
}

#pragma callee_saves screenPrvSleepTillDone
static void screenPrvSleepTillDone(void)
{
	uint8_t ien0, ien1, ien2;

	PICTL &= (uint8_t)~(1 << 1);	//port 1 interupts on rising edge
	P1IEN |= 1 << 0;				//port 1 pin 0 interrupts
	
	(void)P1;						//read port;
	P1IFG &= (uint8_t)~(1 << 0);	//clear int flag in port
	(void)P1IFG;
	IRCON2 &= (uint8_t)~(1 << 3);	//clear P1 int flag in int controller
	
	ien0 = IEN0;
	IEN0 = 0;
	ien1 = IEN1;
	IEN1 = 0;
	ien2 = IEN2;
	IEN2 = (uint8_t)(1 << 4);					//p1 int only
	IEN0 = (uint8_t)(1 << 7);					//ints in general are on
	
	SLEEP = (SLEEP & (uint8_t)~(3 << 0)) | (0 << 0);	//sleep in pm0
	
	sleepTillInt();
	
	P1IEN &= (uint8_t)~(1 << 0);	//port 1 pin 0 interrupts
	P1IFG &=(uint8_t)~(1 << 0);		//clear int flag in port
	IRCON2 &=(uint8_t)~(1 << 3);	//clear P1 int flag in int controller
	
	IEN2 = ien2;
	IEN1 = ien1;
	IEN0 = ien0;

	//just in case we're not done...
	while (!(P1 & (1 << 0)));
}


#pragma callee_saves einkDeselect
static void screenPrvSendLut(uint8_t cmd, const uint8_t __code *ptr, uint8_t len, uint8_t sendLen) __reentrant
{
	einkSelect();
	screenPrvSendCommand(cmd);
	
	while (len--) {
		sendLen--;
		screenPrvSendData(*ptr++);
	}
	
	while (sendLen--)
		screenPrvSendData(0);
	
	einkDeselect();
}

#pragma callee_saves screenPrvSetStage1Luts
static void screenPrvSetStage1Luts(void)
{
	screenPrvSendLut(0x20, mLutStage1_Vcom, sizeof(mLutStage1_Vcom), 60);
	//i do not know why, but these need to be sen tin this order!!!
	screenPrvSendLut(0x22, mLutStage1_10, sizeof(mLutStage1_10), 60);
	screenPrvSendLut(0x23, mLutStage1_01, sizeof(mLutStage1_01), 36);
	screenPrvSendLut(0x24, mLutStage1_00, sizeof(mLutStage1_00), 36);
}

#pragma callee_saves screenPrvSetStage2Luts
static void screenPrvSetStage2Luts(void)
{
	screenPrvSendLut(0x20, mLutStage2_Vcom, sizeof(mLutStage2_Vcom), 60);
	//i do not know why, but these need to be sen tin this order!!!
	screenPrvSendLut(0x22, mLutStage2_10, sizeof(mLutStage2_10), 60);
	screenPrvSendLut(0x23, mLutStage2_01, sizeof(mLutStage2_01), 36);
	screenPrvSendLut(0x24, mLutStage2_00, sizeof(mLutStage2_00), 36);
}

#pragma callee_saves screenPrvSetStage3Luts
static void screenPrvSetStage3Luts(void)
{
	screenPrvSendLut(0x20, mLutStage3_Vcom, sizeof(mLutStage3_Vcom), 60);
	//i do not know why, but these need to be sen tin this order!!!
	screenPrvSendLut(0x22, mLutStage3_10, sizeof(mLutStage3_10), 60);
	screenPrvSendLut(0x23, mLutStage3_01, sizeof(mLutStage3_01), 36);
	screenPrvSendLut(0x24, mLutStage3_00, sizeof(mLutStage3_00), 36);
}

#pragma callee_saves screenPrvSetStage4Luts
static void screenPrvSetStage4Luts(void)
{
	screenPrvSendLut(0x20, mLutStage4_Vcom, sizeof(mLutStage4_Vcom), 60);
	//i do not know why, but these need to be sen tin this order!!!
	screenPrvSendLut(0x22, mLutStage4_10, sizeof(mLutStage4_10), 60);
	screenPrvSendLut(0x23, mLutStage4_01, sizeof(mLutStage4_01), 36);
	screenPrvSendLut(0x24, mLutStage4_00, sizeof(mLutStage4_00), 36);
}

#pragma callee_saves screenPrvSetPartialLuts
static void screenPrvSetPartialLuts(void)
{
	screenPrvSendLut(0x20, mLutPartial_Vcom, sizeof(mLutPartial_Vcom), 60);
	//i do not know why, but these need to be sen tin this order!!!
	screenPrvSendLut(0x22, mLutPartial_10, sizeof(mLutPartial_10), 60);
	screenPrvSendLut(0x23, mLutPartial_01, sizeof(mLutPartial_01), 36);
	screenPrvSendLut(0x24, mLutPartial_00, sizeof(mLutPartial_00), 36);
}

#pragma callee_saves screenPrvConfigVoltages
static void screenPrvConfigVoltages(__bit weakVdl)
{
	einkSelect();
	screenPrvSendCommand(0x02);
	einkDeselect();
	
	//wait for not busy
	while (!(P1 & (1 << 0)));
	
	timerDelay(TIMER_TICKS_PER_SECOND / 5);	//wait 200 ms
	
	einkSelect();
	screenPrvSendCommand(0x01);
	screenPrvSendData(0x03);
	screenPrvSendData(0x00);
	screenPrvSendData(0x2b);						//+15.0V
	screenPrvSendData(weakVdl ? 0x00 : 0x2b);		//-15.0V or -6.2V
	screenPrvSendData(0x09);						//+4.2V
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x04);
	einkDeselect();
	
	//wait for not busy
	while (!(P1 & (1 << 0)));
}

#pragma callee_saves screenInitIfNeeded
static void screenInitIfNeeded(__bit forPartial)
{
	if (mInited)
		return;
	
	mInited = true;
	mPartial = forPartial;
	
	//pins are gpio
	P0SEL &= (uint8_t)~((1 << 0) | (1 << 6) | (1 << 7));
	P1SEL = (P1SEL & (uint8_t)~((1 << 0) | (1 << 1) | (1 << 2))) | (1 << 3) | (1 << 5);
	
	//directions set as needed
	P0DIR |= (1 << 0) | (1 << 6) | (1 << 7);
	P1DIR = (P1DIR & (uint8_t)~(1 << 0)) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 5);
	
	//default state set (incl keeping it in reset and disabled, data mode selected)
	P0 = (P0 & (uint8_t)~(1 << 0)) | (1 << 6) | (1 << 7);
	P1 = (P1 & (uint8_t)~((1 << 2) | (1 << 3) | (1 << 5))) | (1 << 1);
	
	//configure the uart0 (alt2, spi, fast)
	PERCFG |= (1 << 0);
	U0BAUD = 0;			//F/8 is max for spi - 3.25 MHz
	U0GCR = 0b00110001;	//BAUD_E = 0x11, msb first
	U0CSR = 0b00000000;	//SPI master mode, RX off
	
	P2SEL &= (uint8_t)~(1 << 6);
	
	//turn on the eInk power (keep in reset for now)
	P0 &= (uint8_t)~(1 << 6);
	timerDelay(TIMER_TICKS_PER_SECOND * 10 / 1000);	//wait 10ms
	
	//release reset
	P1 |= 1 << 2;
	timerDelay(TIMER_TICKS_PER_SECOND * 10 / 1000);	//wait 10ms
	
	//wait for not busy
	while (!(P1 & (1 << 0)));
	
	einkSelect();
	screenPrvSendCommand(0x03);
	screenPrvSendData(0x00);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x06);
	screenPrvSendData(0x17);
	screenPrvSendData(0x17);
	screenPrvSendData(0x1e);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x30);
	screenPrvSendData(0x21);
	einkDeselect();
	
	screenPrvConfigVoltages(false);
	
	einkSelect();
	screenPrvSendCommand(0x00);
	screenPrvSendData(0xaf);
	screenPrvSendData(0x89);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x41);
	screenPrvSendData(0x00);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x50);
	screenPrvSendData(0xd7);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x60);
	screenPrvSendData(0x22);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x82);
	screenPrvSendData(0x12);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x2a);
	screenPrvSendData(0x80);
	screenPrvSendData(0x00);
	einkDeselect();
	
	if (forPartial)
		screenPrvSetPartialLuts();
	else
		screenPrvSetStage1Luts();
	
	einkSelect();
	screenPrvSendCommand(0x61);
	screenPrvSendData(SCREEN_WIDTH & 0xff);
	screenPrvSendData(SCREEN_HEIGHT >> 8);
	screenPrvSendData(SCREEN_HEIGHT & 0xff);
	einkDeselect();
}

void screenShutdown(void)
{
	if (!mInited)
		return;
	
	einkSelect();
	screenPrvSendCommand(0x02);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x07);
	screenPrvSendData(0xa5);
	einkDeselect();
	
	P0 |= (1 << 6);
	
	mInited = false;
}

void screenTest(void)
{
	uint8_t iteration, c;
	uint16_t __pdata r;
	
	if (!screenTxStart(false)) {
		pr("fail to init\n");
		return;
	}
	
	for (iteration = 0; iteration < SCREEN_DATA_PASSES; iteration++) {

		for (r = 0; r < SCREEN_HEIGHT; r++) {
		
			uint8_t val = 0, npx = 0, rc = (r >> 4);
		
			rc = mathPrvMul8x8(rc, rc + 1) >> 1;
		
			for (c = 0; c != (uint8_t)SCREEN_WIDTH; c++) {
				
				uint8_t color = (uint8_t)(rc + (c >> 4)) % (SCREEN_NUM_GREYS + 1);
				
				val <<= SCREEN_TX_BPP;
				val += color;
				npx += SCREEN_TX_BPP;
				
				if (npx == 8) {
					screenByteTx(val);
					npx = 0;
				}
			}
		}
		screenEndPass();
	}
	
	screenTxEnd();
	pr("done\n");
}

__bit screenTxStart(__bit forPartial)
{
	screenInitIfNeeded(forPartial);
	mPassNo = 0;
	
	einkSelect();
	screenPrvSendCommand(0x10);
	
	return true;
}

static void screenPrvDraw(void)
{
	einkSelect();
	screenPrvSendCommand(0x12);
	einkDeselect();
	
	timerDelay(TIMER_TICKS_PER_SECOND / 10);
	
	screenPrvSleepTillDone();
}

void screenEndPass(void)
{
	switch (mPassNo) {
		case 0:
		case 2:
		case 4:
		case 6:
			einkDeselect();
			einkSelect();
			screenPrvSendCommand(0x13);
			break;
	#if SCREEN_DATA_PASSES > 2
		case 1:
			einkDeselect();
			if (mPartial)		//will keep us in "stage 1" with data deselected safely
				return;
			screenPrvDraw();
			screenPrvConfigVoltages(true);
			screenPrvSetStage2Luts();
			einkSelect();
			screenPrvSendCommand(0x10);
			break;
	#endif
	#if SCREEN_DATA_PASSES > 4
		case 3:
			einkDeselect();
			screenPrvDraw();
			screenPrvSetStage3Luts();
			einkSelect();
			screenPrvSendCommand(0x10);
			break;
	#endif
	#if SCREEN_DATA_PASSES > 6
		case 5:
			einkDeselect();
			screenPrvDraw();
			screenPrvConfigVoltages(false);
			screenPrvSetStage4Luts();
			einkSelect();
			screenPrvSendCommand(0x10);
			break;
	#endif
		default:
			return;
	}
	mPassNo++;
}

//first 2 passes will do activation, and treat greys as white (00->00=black, 01->01=white, 10->01=white, 11->10=red)
//second 2 passes will develop reds and mid-grey (black and white treated as no-op) (00->00=no-op, 01->01=midgrey, 10->00=no-op, 11->10=red)
//third 2 passes will lighten 2 lightest greys, darken two darkest greys, keep others
//fourth 2 passes will lighten lightest grey, darken second-lightest, darken darkest grey, lighten second-darkest, keep others

//in any case 2 passes are used to get the data into the screen for each "draw" op, since data s given to us interleaved

/*
	input nibble (only 7 bits significant) to per-pass 1-bit value

	px		0	1	2	3	4	5
	
bk	0000	1	0	0	1	0	1
	0001	1	0	0	1	1	0
	0010	1	0	0	1	0	0
xx	0011	1	0	1	0	0	1
	0100	1	0	1	0	1	0
	0101	1	0	1	0	0	0
wh	0110	1	0	0	0	0	0
rd	0111	0	1	0	0	0	0

*/

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte)
{
	static const uint8_t __code extractPass0[] = {0,1,1,1,0};
	static const uint8_t __code extractPass1[] = {0,0,0,0,1};
	static const uint8_t __code extractPass2[] = {0,0,1,0,0};
	static const uint8_t __code extractPass3[] = {0,1,0,0,0};
	static const uint8_t __code extractPass4[] = {1,0,0,0,0};
	static const uint8_t __code extractPass5[] = {0,0,0,0,1};
	static const uint8_t __code extractPass6[] = {0,0,0,1,0};
	static const uint8_t __code extractPass7[] = {0,0,0,0,0};
	static const uint8_t __code * __code extractPass[] = {extractPass0, extractPass1, extractPass2, extractPass3, extractPass4, extractPass5, extractPass6, extractPass7};
	const uint8_t __code *curExtractPass = extractPass[mPassNo];
	static uint8_t __xdata prev, step = 0;
	
	prev = (prev << 2) | (curExtractPass[byte >> 4] << 1) | curExtractPass[byte & 0x0f];
	if (++step == 4) {
		step = 0;
		screenPrvSendData(prev);
	}
}

void screenTxEnd(void)
{
	einkDeselect();
	
	screenPrvDraw();
	
	screenShutdown();
}

