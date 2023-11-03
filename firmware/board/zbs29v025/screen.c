#include <stdbool.h>
#include "asmUtil.h"
#include "screen.h"
#include "printf.h"
#include "sleep.h"
#include "board.h"
#include "timer.h"
#include "adc.h"
#include "cpu.h"
#include "spi.h"

uint8_t __xdata mScreenRow[320];

uint8_t __xdata mScreenVcom;

/*
	

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

	0x90, 0x10, 0x03, 0x05, 0x00, 0x34,
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


#define einkPrvSelect()			\
	do{							\
		P1_7 = 0;				\
	} while (0)

#define einkPrvDeselect()		\
	do{							\
		P1_7 = 1;				\
	} while (0)

#define einkPrvMarkCommand()	\
	do{							\
		P2_2 = 0;				\
	} while (0)

#define einkPrvMarkData()		\
	do{							\
		P2_2 = 1;				\
	} while (0)
	

#pragma callee_saves einkPrvCmd
static void einkPrvCmd(uint8_t cmd)	//sets chip select
{
	einkPrvMarkCommand();
	spiByte(cmd);
}

#pragma callee_saves einkPrvData
static void einkPrvData(uint8_t byte)
{
	einkPrvMarkData();
	spiByte(byte);
}

#pragma callee_saves einkPrvWaitWithTimeout
static void einkPrvWaitWithTimeout(uint32_t timeout)
{
	uint32_t __xdata start = timerGet();
	
	while (timerGet() - start < timeout) {
		
		if (P2_1)
			return;
	}
	pr("screen timeout %lu ticks\n", timerGet() - start);
	while(1);
}

#pragma callee_saves einkPrvDeselect
static void screenPrvSendLut(uint8_t cmd, const uint8_t __code *ptr, uint8_t len, uint8_t sendLen) __reentrant
{
	einkPrvSelect();
	einkPrvCmd(cmd);
	
	while (len--) {
		sendLen--;
		einkPrvData(*ptr++);
	}
	
	while (sendLen--)
		einkPrvData(0);
	
	einkPrvDeselect();
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
	einkPrvSelect();
	einkPrvCmd(0x02);
	einkPrvDeselect();
	
	//wait for not busy
	einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND / 10);
	
	timerDelay(TIMER_TICKS_PER_SECOND / 5);	//wait 200 ms
	
	einkPrvSelect();
	einkPrvCmd(0x01);
	einkPrvData(0x03);
	einkPrvData(0x00);
	einkPrvData(0x26);
	einkPrvData(weakVdl ? 0x09 : 0x26);
	einkPrvData(0x09);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x04);
	einkPrvDeselect();
	
	//wait for not busy
	einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND / 10);
}

#pragma callee_saves screenInitIfNeeded
static void screenInitIfNeeded(__bit forPartial)
{
	if (mInited)
		return;
	
	mInited = true;
	mPartial = forPartial;

	timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	P2_0 = 0;
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	P2_0 = 1;
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	
	//wait for not busy
	timerDelay(TIMER_TICKS_PER_SECOND / 100);
	
	einkPrvSelect();
	einkPrvCmd(0x03);
	einkPrvData(0x00);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x06);
	einkPrvData(0x17);
	einkPrvData(0x17);
	einkPrvData(0x1e);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x30);
	einkPrvData(0x21);
	einkPrvDeselect();
	
	screenPrvConfigVoltages(false);
	
	einkPrvSelect();
	einkPrvCmd(0x00);
	einkPrvData(0xaf);
	einkPrvData(0x89);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x41);
	einkPrvData(0x00);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x50);
	einkPrvData(0xd7);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x60);
	einkPrvData(0x22);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x82);
	einkPrvData(0x12);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x2a);
	einkPrvData(0x80);
	einkPrvData(0x00);
	einkPrvDeselect();
	
	if (forPartial)
		screenPrvSetPartialLuts();
	else
		screenPrvSetStage1Luts();
	
	einkPrvSelect();
	einkPrvCmd(0x61);
	einkPrvData(SCREEN_WIDTH & 0xff);
	einkPrvData(SCREEN_HEIGHT >> 8);
	einkPrvData(SCREEN_HEIGHT & 0xff);
	einkPrvDeselect();
}

void screenShutdown(void)
{
	if (!mInited)
		return;
	
	einkPrvSelect();
	einkPrvCmd(0x02);
	einkPrvDeselect();
	
	einkPrvSelect();
	einkPrvCmd(0x07);
	einkPrvData(0xa5);
	einkPrvDeselect();
	
	mInited = false;
}

void screenTest(void)
{
	uint8_t iteration, c;
	uint16_t PDATA r;
	
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
	
	einkPrvSelect();
	einkPrvCmd(0x10);
	einkPrvDeselect();
	
	return true;
}

static void screenPrvDraw(void)
{
	einkPrvSelect();
	einkPrvCmd(0x12);
	einkPrvDeselect();
	
	timerDelay(TIMER_TICKS_PER_SECOND / 10);
	einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND * 60);
}

void screenEndPass(void)
{
	switch (mPassNo) {
		case 0:
		case 2:
		case 4:
		case 6:
			einkPrvSelect();
			einkPrvCmd(0x13);
			einkPrvDeselect();
			break;
	#if SCREEN_DATA_PASSES > 2
		case 1:
			if (mPartial)		//will keep us in "stage 1" with data deselected safely
				return;
			screenPrvDraw();
			screenPrvConfigVoltages(true);
			screenPrvSetStage2Luts();
			einkPrvSelect();
			einkPrvCmd(0x10);
			einkPrvDeselect();
			break;
	#endif
	#if SCREEN_DATA_PASSES > 4
		case 3:
			screenPrvDraw();
			screenPrvSetStage3Luts();
			einkPrvSelect();
			einkPrvCmd(0x10);
			einkPrvDeselect();
			break;
	#endif
	#if SCREEN_DATA_PASSES > 6
		case 5:
			screenPrvDraw();
			screenPrvConfigVoltages(false);
			screenPrvSetStage4Luts();
			einkPrvSelect();
			einkPrvCmd(0x10);
			einkPrvDeselect();
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
		einkPrvSelect();
		einkPrvData(prev);
		einkPrvDeselect();
	}
}

void screenTxEnd(void)
{
	einkPrvDeselect();
	
	screenPrvDraw();
	
	screenShutdown();
}

uint16_t adcSampleBattery(void)
{
	return 0;
}


