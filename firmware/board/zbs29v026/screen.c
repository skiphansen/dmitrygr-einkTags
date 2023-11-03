#include <stdbool.h>
#include "asmUtil.h"
#include "screen.h"
#include "printf.h"
#include "board.h"
#include "timer.h"
#include "adc.h"
#include "cpu.h"
#include "spi.h"


uint8_t __xdata mScreenRow[320];


//SSD1675A with some features of 1675B

//luts 0..3 are data, 4 is vcom
//R	B	LUT
//0	0	0
//0	1	1
//1	0	2
//1	1	3

/*
	LUT {		//100 bytes total
		GROUP[10]
	}
	
	GROUP {
		PHASE[4]
		repeatNum(1..256)
	};
	
	PHASE {
		len (0..255)
		voltage data:{vss,vsh1,vsl,vsh2}  vcom:{dcvom,vsh1+dcvcom,vsl+dcvcom}
	}

	

	voltage: VS[nX-LUTm] = group n[0..9] for LUT m[0..4]
	TP[nX] = phase length for group n[0..9]
	RP[n] = repeat count of a group n[0..9] (0->1, 1->2, ...255->256)

	this is all packed rather tightly
	
	first, the voltage levels (4 per byte, msb to lsb, 10 bytes per lut, 5 luts) = 50 bytes
	then for each of the 10 groups {
		u8 pahseLength[4];	//abcd
		u8 repeat
	}

*/



static __bit mInited = false, mPartial;
static uint8_t __xdata mPassNo;





#define SCREEN_CMD_CLOCK_ON					0x80
#define SCREEN_CMD_CLOCK_OFF				0x01

#define SCREEN_CMD_ANALOG_ON				0x40
#define SCREEN_CMD_ANALOG_OFF				0x02

#define SCREEN_CMD_LATCH_TEMPERATURE_VAL	0x20

#define SCREEN_CMD_LOAD_LUT					0x10
#define SCREEN_CMD_USE_MODE_2				0x08	//modified commands 0x10 and 0x04

#define SCREEN_CMD_REFRESH					0x04


	
static const uint8_t __code mLutsPhase0[] = {
	//lut0 (black) voltages
	0x0a, 0x90, 0x90, 0x10, 0x10, 0x00, 0x00,
	//lut1 (white) voltages
	0x01, 0x90, 0x90, 0x80, 0x80, 0x00, 0x00,
	//lut2 (red) voltages
	0xaa, 0x90, 0x90, 0x90, 0x00, 0xb0, 0xc0,
	//lut3 (greys) voltages
	0x0a, 0x90, 0x90, 0x80, 0x80, 0x00, 0x00,
	//lut4 (vcom) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group0 phase lengths and repeat count
	0x0f, 0x16, 0x1f, 0x3e, 0x02 - 1,

	//group1 phase lengths and repeat count
	0x68, 0x68, 0x00, 0x00, 0x0c - 1,
	
	//group2 phase lengths and repeat count
	0x0a, 0x0a, 0x00, 0x00, 0x19 - 1,
	
	//group3 phase lengths and repeat count
	0x02, 0x03, 0x00, 0x00, 0x19 - 1,
	
	//group4 phase lengths and repeat count
	0x02, 0x03, 0x00, 0x00, 0x06 - 1,
	
	//group5 phase lengths and repeat count
	0x05, 0x48, 0x00, 0x00, 0x09 - 1,
	
	//group6 phase lengths and repeat count
	0x8e, 0x00, 0x00, 0x00, 0x04 - 1,
};

static const uint8_t __code mLutsPhase1[] = {
	//lut0 (dark grey) voltages
	0x95, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut1 (middle grey) voltages
	0x91, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut2 (light grey) voltages
	0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut3 (KEEP) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut4 (vcom) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group0 phase lengths and repeat count
	0x1b, 0x01, 0x02, 0x01, 0x30 - 1,

	//group1 not used
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group2 not used
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group3 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group4 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group5 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group6 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutsPhase2[] = {
	//lut0 (black) voltages
	0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut1 (white) voltages
	0x8a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut2 (red) voltages
	0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut3 (keep) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut4 (vcom) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group0 phase lengths and repeat count
	0x01, 0x80, 0x04, 0x01, 0x02 - 1,

	//group1 phase lengths and repeat count
	0x11, 0x05, 0x11, 0x05, 0x10 - 1,
	
	//group2 not used
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group3 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group4 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group5 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group6 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutsPartial[] = {
	//lut0 (KEEP) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut1 (W2B) voltages
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut2 (B2W) voltages
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut3 (unused) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//lut4 (vcom) voltages
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

	//group0 phase lengths and repeat count
	0x10, 0x02, 0x00, 0x00, 0x03 - 1,

	//group1 not used
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group2 not used
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group3 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group4 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group5 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
	
	//group6 phase lengths and repeat count
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code colorMap[][2][6] =
{	//colors are: B, DG, G, LG, W, R
	//phase 0 (LUTS: B:W:R:G, purpose: BWR, prepare greys)
	{
		{0, 1, 1, 1, 1, 0, },	//lo plane (B)
		{0, 1, 1, 1, 0, 1, },	//hi plane (R)
	},
	//phase 1 (LUTS: DG:G:LG:_, purpose: greys)
	{
		{1, 0, 1, 0, 1, 1, },	//lo plane (B)
		{1, 0, 0, 1, 1, 1, },	//hi plane (R)
	},
	//phase 0 (LUTS: B:W:R:_, purpose: re-firm BWR)
	{
		{0, 1, 1, 1, 1, 0, },	//lo plane (B)
		{0, 1, 1, 1, 0, 1, },	//hi plane (R)
	},
};

static const uint8_t __code mVsl[] =
{
	0x2e,	//partial (-12V)
	0x2e,	//phase 0 (-12V)
	0x0c,	//phase 1 (around 6v)
	0x2e,	//phase 2 (-12V)
};

static const uint8_t __code * __code mLuts[] =
{
	mLutsPartial,
	mLutsPhase0,
	mLutsPhase1,
	mLutsPhase2,
};

static const uint8_t __code mColorMapPartial[][6] =
{	//colors are: B, DG, G, LG, W, R
	
	//partial (LUTS: KEEP, W2B, B2W, unused)
	{
		0, 1, 0, 0, 0, 0,	//lo plane (B)
	},
	{
		0, 0, 1, 0, 0, 0,	//hi plane (R)
	},
};

static const uint8_t __code mColorMap[][6] =
{
	//colors are: B, DG, G, LG, W, R
	//phase 0 (LUTS: B:W:R:G, purpose: BWR, prepare greys)
	{
		0, 1, 1, 1, 1, 0,	//lo plane (B)
	},
	{
		0, 1, 1, 1, 0, 1,	//hi plane (R)
	},
	//phase 1 (LUTS: DG:G:LG:_, purpose: greys)
	{
		1, 0, 1, 0, 1, 1,	//lo plane (B)
	},
	{
		1, 0, 0, 1, 1, 1,	//hi plane (R)
	},
	//phase 0 (LUTS: B:W:R:_, purpose: re-firm BWR)
	{
		0, 1, 1, 1, 1, 0,	//lo plane (B)
	},
	{
		0, 1, 1, 1, 0, 1,	//hi plane (R)
	},
};



#define einkPrvSelect()			\
	do{							\
		P1_7 = 0;				\
	} while (0)

#define einkPrvDeselect()		\
	do{							\
		P1_7 = 1;				\
	} while (0)
//urx pin
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
	einkPrvSelect();
	einkPrvMarkCommand();
	spiByte(cmd);
}

#pragma callee_saves einkPrvData
static void einkPrvData(uint8_t byte)
{
	einkPrvMarkData();
	spiByte(byte);
}

#pragma callee_saves einkPrvCmdWithOneByte
static void einkPrvCmdWithOneByte(uint16_t vals)	//passing in one u16 is better than two params cause SDCC sucks
{
	einkPrvCmd(vals >> 8);
	einkPrvData(vals);
	einkPrvDeselect();
}

#pragma callee_saves einkPrvWaitWithTimeout
static void einkPrvWaitWithTimeout(uint32_t timeout)
{
	uint32_t __xdata start = timerGet();
	
	while (timerGet() - start < timeout) {
		
		if (!P2_1)
			return;
	}
	pr("screen timeout %lu ticks\n", timerGet() - start);
	while(1);
}

#pragma callee_saves einkPrvReadByte
static uint8_t einkPrvReadByte(void)
{
	uint8_t val = 0, i;
	
	P0DIR = (P0DIR &~ (1 << 0)) | (1 << 1);
	P0 &=~ (1 << 0);
	P0FUNC &=~ ((1 << 0) | (1 << 1));
	
	P2_2 = 1;
	
	for (i = 0; i < 8; i++) {
		P0_0 = 1;
		__asm__("nop\nnop\nnop\nnop\nnop\n");
		val <<= 1;
		if (P0_1)
			val++;
		P0_0 = 0;
		__asm__("nop\nnop\nnop\nnop\nnop\n");
	}
	
	
	//set up pins for spi (0.0,0.1,0.2)
	P0FUNC |= (1 << 0) | (1 << 1);
	
	return val;
}

#pragma callee_saves einkPrvReadStatus
static uint8_t einkPrvReadStatus(void)
{
	uint8_t sta;
	einkPrvCmd(0x2f);
	
	sta = einkPrvReadByte();
	einkPrvDeselect();
	
	return sta;
}

#pragma callee_saves einkPrvReadStatus
static void screenPrvStartPhase(int8_t phase)
{
	uint8_t i;
		
	phase++;
	
	einkPrvCmdWithOneByte(0x030f);	//VGH/VGL = +/-16V
		
	einkPrvCmdWithOneByte(0x2c50);	//VCOM = -2.0V
	
	einkPrvCmdWithOneByte(0x3a0c);	//frame rate 90hz
	einkPrvCmdWithOneByte(0x3b07);	//as above
	
	
	einkPrvCmd(0x0c);
	einkPrvData(0x8f);
	einkPrvData(0x8f);
	einkPrvData(0x8f);
	einkPrvData(0x3f);
	einkPrvDeselect();
	
	//VSL
	einkPrvCmd(0x04);
	einkPrvData(0x3c);	//VSH1 = 14V
	einkPrvData(0xa3);	//VSH2 = 4.5V
	einkPrvData(mVsl[phase]);
	einkPrvDeselect();
	
	//LUTs
	einkPrvCmd(0x32);
	for (i = 0; i < 70; i++)
		einkPrvData(mLuts[phase][i]);
	einkPrvDeselect();
}

#pragma callee_saves screenPrvStartSubPhase
static void screenPrvStartSubPhase(__bit redSubphase)
{
	einkPrvCmd(0x4e);
	einkPrvData(0);
	einkPrvDeselect();
	
	einkPrvCmd(0x4f);
	einkPrvData(0x00);
	einkPrvData(0x00);
	einkPrvDeselect();

	einkPrvCmd(redSubphase ? 0x26 : 0x24);
	
	einkPrvDeselect();
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
	
	einkPrvCmd(0x12);	//software reset
	einkPrvDeselect();
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	
	einkPrvCmdWithOneByte(0x7454);
	
	einkPrvCmdWithOneByte(0x7e3b);
	
	einkPrvCmd(0x2b);
	einkPrvData(0x04);
	einkPrvData(0x63);
	einkPrvDeselect();
	
	einkPrvCmd(0x0c);	//they send 8f 8f 8f 3f
	einkPrvData(0x8b);
	einkPrvData(0x9c);
	einkPrvData(0x96);
	einkPrvData(0x0f);
	einkPrvDeselect();
	
	einkPrvCmd(0x01);
	einkPrvData((SCREEN_HEIGHT - 1) & 0xff);
	einkPrvData((SCREEN_HEIGHT - 1) >> 8);
	einkPrvData(0x00);
	einkPrvDeselect();
	
	//turn on clock & analog
	einkPrvCmdWithOneByte(0x2200 | SCREEN_CMD_CLOCK_ON | SCREEN_CMD_ANALOG_ON);
	einkPrvCmd(0x20);				//do action
	einkPrvDeselect();
	einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND / 10);
	
	einkPrvCmdWithOneByte(0x1103);
	
	einkPrvCmd(0x44);
	einkPrvData(0x00);
	einkPrvData(SCREEN_WIDTH / 8 - 1);
	einkPrvDeselect();
	
	einkPrvCmd(0x45);
	einkPrvData(0x00);
	einkPrvData(0x00);
	einkPrvData((SCREEN_HEIGHT - 1) & 0xff);
	einkPrvData((SCREEN_HEIGHT - 1) >> 8);
	einkPrvDeselect();
	
	einkPrvCmdWithOneByte(0x3cc0);	//border will be HiZ
	
	einkPrvCmdWithOneByte(0x1880);	//internal temp sensor
	
	screenPrvStartPhase(forPartial ? -1 : 0);
}

#pragma callee_saves screenPrvDraw
static void screenPrvDraw(void)
{
	einkPrvCmdWithOneByte(0x2200 | SCREEN_CMD_REFRESH);
	einkPrvCmd(0x20);				//do actions
	einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND * 60UL);
}

__bit screenTxStart(__bit forPartial)
{
	screenInitIfNeeded(forPartial);
	mPassNo = 0;
	
	screenPrvStartSubPhase(false);

	return true;
}

void screenEndPass(void)
{
	switch (mPassNo) {
		case 0:
		case 2:
		case 4:
			screenPrvStartSubPhase(true);
			break;
	
	#if SCREEN_DATA_PASSES > 6
		case 5:
	#endif
	#if SCREEN_DATA_PASSES > 4
		case 3:
	#endif
	#if SCREEN_DATA_PASSES > 2
		case 1:
			if (mPartial)		//will keep us in "stage 1" with data deselected safely
				return;
			screenPrvDraw();
			screenPrvStartPhase((mPassNo >> 1) + 1);
			screenPrvStartSubPhase(false);
			break;
	#endif
		default:
			return;
	}
	mPassNo++;
}

void screenTxEnd(void)
{
	screenPrvDraw();
	screenShutdown();
}

void screenShutdown(void)
{
	if (!mInited)
		return;
	
	mInited = false;
	einkPrvCmdWithOneByte(0x1003);	//shut down
}


#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte)
{
	static uint8_t __xdata prev, step = 0;
	
	//XXX: partial
	
	prev <<= 2;
	if (mPartial)
		prev |= (mColorMapPartial[mPassNo][byte >> 4] << 1) | mColorMapPartial[mPassNo][byte & 0x0f];
	else
		prev |= (mColorMap[mPassNo][byte >> 4] << 1) | mColorMap[mPassNo][byte & 0x0f];
	if (++step == 4) {
		step = 0;
		einkPrvSelect();
		einkPrvData(prev);
		einkPrvDeselect();
	}
}

//yes this is here...
uint16_t adcSampleBattery(void)
{
	__bit wasInited = mInited;
	uint16_t voltage = 2600;
	
	if (!mInited)
		screenInitIfNeeded(true);
	
	uint8_t val;
	
	for (val = 3; val < 8; val++) {
		
		einkPrvCmdWithOneByte(0x1500 + val);
		einkPrvWaitWithTimeout(TIMER_TICKS_PER_SECOND / 10);
		if (einkPrvReadStatus() & 0x10)	{//set if voltage is less than threshold ( == 1.9 + val / 10)
			voltage = 1850 + mathPrvMul8x8(val, 100);
			break;
		}
	}
	
	if (!wasInited)
		screenShutdown();
	
	return voltage;
}
