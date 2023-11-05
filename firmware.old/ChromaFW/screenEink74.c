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
	
	
	//controller is 99.9% likely to be UC8159 version c_B
	
	//LUT format info from: https://www.buydisplay.com/download/ic/UC8159C.pdf


		VCOM LUT is: 20x VcomLutInfo
		
			VcomLutInfo {
				u8 numRepeats;		//how many times to perform this LUT step
				u2 level[8];		//MSB to LSB. 00 = VCM_DC, 01 = 15V + VCM_DC, 10 = -15V + VCM_DC, 11 = float
				u8 numFrames[8];	//for each level
			}


		COLOR LUTs ARE: 20x ColorLutInfo
		
			ColorLutInfo {
				u8 numRepeats;		//how many times to perform this LUT step
				u4 level[8];		//MSB to LSB. top bit ignored. 000 = VCM_DC, 001 = 15V (VSH), 010 = -15V (VSL), 011 = VSH_LV, 100 = VSL_LV, 101 = VSH_LVX, 110 = VSL_LVX, 111 = float
				u8 numFrames[8];	//for each level
			}
		
		
		XON LUT is: 20x XonLutInfo
		
			XonLutInfo {
				u8 numRepeats;		//how many times to perform this LUT step
				u1 level[8];		//MSB to LSB. 0 = all gates ON, 1 = normal gate scan
				u8 numFrames[8];	//for each level
			}
*/

__xdata __at (0xfda2) uint8_t mScreenRow[320];	//350 bytes here, we use 320 of them

struct LutCorrectionRange {	//all possible ranges must be covered
	int8_t minTemp;			//inclusive
	int8_t maxTemp;			//inclusive
	uint8_t vals[4];		
};

struct LutCorrectionEntry {
	uint16_t offset;		//0xffff for end of list
	uint8_t len;			//up to 4
	const struct LutCorrectionRange __code *ranges;
};

static __bit mInited = false;

#define LUT_CMD_000	0x21
#define LUT_CMD_001	0x23
#define LUT_CMD_010	0x24
#define LUT_CMD_011	0x22
#define LUT_CMD_100	0x25
#define LUT_CMD_101	0x26
#define LUT_CMD_110	0x27
#define LUT_CMD_111	0x28

#define SET_LUT(_idx, _nm, _len, _corrections)	screenPrvSendLut(_idx, _nm, sizeof(_nm), _len, false, _corrections);
#define SET_COLOR_LUT(_idx, _nm, _corrections)	SET_LUT(LUT_CMD_ ## _idx, _nm, 260, _corrections);
#define SET_XON_LUT(_nm)						screenPrvSendLut(0x29, _nm, sizeof(_nm), 200, true, 0);

extern int LinkError(void);

#define VERIFY_SUM8(_v1, _v2, _v3, _v4, _v5, _v6, _v7, _v8, _sum)	(_v1), (_v2), (_v3), (_v4), (_v5), (_v6), (_v7), (_v8) + (((_v1) + (_v2) + (_v3) + (_v4) + (_v5) + (_v6) + (_v7) + (_v8) != (_sum)) ? LinkError() : 0)

#define NUM_REPS_INITIAL_CHILL		1
#define NUM_FRMS_INITIAL_CHILL		0xc8

#define NUM_REPS_DC_BALANCE			1
#define NUM_FRMS_DC_BALANCE			9

#define NUM_REPS_ACTIVATE_1			29
#define NUM_FRMS_ACTIVATE_1			19

#define NUM_REPS_ACTIVATE_SLOW		16
#define NUM_FRMS_ACTIVATE_SLOW		0x98

#define NUM_REPS_ACTIVATE_FAST		50
#define NUM_FRMS_ACTIVATE_FAST		12

#define NUM_REPS_ACTIVATE_MED		5
#define NUM_FRMS_ACTIVATE_MED		20

#define NUM_REPS_COARSE_GREYS		22
#define NUM_FRMS_COARSE_GREYS		8

#define NUM_REPS_DEVELOP_YELLOWS_1	6
#define NUM_FRMS_DEVELOP_YELLOWS_1	0x58

#define NUM_REPS_DEVELOP_Y2_n_G		5			//becomes 4 for temp <= 18
#define NUM_FRMS_DEVELOP_Y2_n_G		0x7d

#define NUM_REPS_LET_IT_SETTLE		2
#define NUM_FRMS_LET_IT_SETTLE		0x50

static const uint8_t __code mLutVcom[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x00, 0x00, VERIFY_SUM8(0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x00, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	NUM_REPS_ACTIVATE_SLOW,		0x00, 0x00, VERIFY_SUM8(0x4C, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_SLOW),
	NUM_REPS_ACTIVATE_FAST,		0x00, 0x00, VERIFY_SUM8(0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_FAST),
	NUM_REPS_ACTIVATE_MED,		0x00, 0x00, VERIFY_SUM8(0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_MED),
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x00, 0x00, VERIFY_SUM8(0x50, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x00, 0x00, VERIFY_SUM8(0x50, 0x28, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, VERIFY_SUM8(0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

#define ACTIVATION_COMMON																														\
	NUM_REPS_ACTIVATE_SLOW,		0x41, 0x20, 0x00, 0x00, VERIFY_SUM8(0x20, 0x3C, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_SLOW),	\
	NUM_REPS_ACTIVATE_FAST,		0x12, 0x00, 0x00, 0x00, VERIFY_SUM8(0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_FAST),	\
	NUM_REPS_ACTIVATE_MED,		0x12, 0x00, 0x00, 0x00, VERIFY_SUM8(0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_MED),


static const uint8_t __code mLutB[] = {

	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x20, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x02, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x0c, 0x06, 0x0c, 0x06, 0x0c, 0x06, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutG0[] = {

	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x20, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x08, 0x0a, 0x08, 0x0a, 0x08, 0x0a, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutG1[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x20, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x06, 0x0c, 0x06, 0x0c, 0x06, 0x0c, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutG2[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x20, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x05, 0x0d, 0x05, 0x0d, 0x05, 0x0d, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutG3[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x20, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x04, 0x0e, 0x04, 0x0e, 0x04, 0x0e, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutG4[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x20, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x42, 0x10, 0x10, 0x10, VERIFY_SUM8(0x2d, 0x1a, 0x02, 0x10, 0x02, 0x10, 0x03, 0x0f, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutW[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x01, 0x10, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x12, 0x00, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x20, 0x00, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x50, 0x2a, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const uint8_t __code mLutBrightYellow[] = {
	
	NUM_REPS_INITIAL_CHILL,		0x42, 0x00, 0x00, 0x00, VERIFY_SUM8(0x94, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0x00, 0x20, 0x00, 0x00, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0x11, 0x02, 0x20, 0x00, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	ACTIVATION_COMMON
	NUM_REPS_COARSE_GREYS,		0x03, 0x30, 0x00, 0x00, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0x30, 0x20, 0x00, 0x00, VERIFY_SUM8(0x50, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0x30, 0x20, 0x00, 0x00, VERIFY_SUM8(0x50, 0x28, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0x00, 0x00, 0x00, 0x00, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};

static const struct LutCorrectionRange __code corrYellowStageTwoRanges[] = {
	{
		.minTemp = -128,
		.maxTemp = 25,
		.vals = {0x60, 0x18, },
	},
	{
		.minTemp = 26,
		.maxTemp = 127,
		.vals = {0x62, 0x16, },
	},
};

static const struct LutCorrectionEntry __code corrYellowStageTwo[] = {
	[0] = {
		
		//first two durations in DEVELOP_Y2_n_G phase change with temp quite a bit
		
		.offset = 13 * 8 + 5,
		.len = 2,
		.ranges = corrYellowStageTwoRanges,
	},
	[1] = {
		.offset = 0xffff,
	},
};

static const uint8_t __code mLutXon[] = {

	NUM_REPS_INITIAL_CHILL,		0xFF, VERIFY_SUM8(0x64, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_INITIAL_CHILL),
	NUM_REPS_DC_BALANCE,		0xFF, VERIFY_SUM8(0x01, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DC_BALANCE),
	NUM_REPS_ACTIVATE_1,		0xFF, VERIFY_SUM8(0x03, 0x01, 0x03, 0x05, 0x07, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_1),
	NUM_REPS_ACTIVATE_SLOW,		0xFF, VERIFY_SUM8(0x4C, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_SLOW),
	NUM_REPS_ACTIVATE_FAST,		0xFF, VERIFY_SUM8(0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_FAST),
	NUM_REPS_ACTIVATE_MED,		0xFF, VERIFY_SUM8(0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_ACTIVATE_MED),
	NUM_REPS_COARSE_GREYS,		0xFF, VERIFY_SUM8(0x02, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_COARSE_GREYS),
	NUM_REPS_DEVELOP_YELLOWS_1,	0xFF, VERIFY_SUM8(0x50, 0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_YELLOWS_1),
	NUM_REPS_DEVELOP_Y2_n_G,	0xFF, VERIFY_SUM8(0x50, 0x28, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_DEVELOP_Y2_n_G),
	NUM_REPS_LET_IT_SETTLE,		0xFF, VERIFY_SUM8(0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, NUM_FRMS_LET_IT_SETTLE),
};



static const uint8_t __code mLutFastVcom[] = {
	
	0x04, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutFastB[] = {
	0x04, 0x10, 0x00, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutFastW[] = {
	0x04, 0x20, 0x00, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutFastNoChange[] = {
	0x04, 0x00, 0x00, 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static const uint8_t __code mLutFastXon[] = {

	0x04, 0xff, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
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

static void screenPrvSendLut(uint8_t cmd, const uint8_t __code *ptr, uint16_t len, uint16_t sendLen, __bit padWithXonConstant /* else zero */, const struct LutCorrectionEntry __code *corrections) __reentrant
{
	uint8_t __xdata *dst = mScreenRow;
	uint16_t origSendLen = sendLen;
	
	sendLen -= len;
	
	//copy/fill buffer
	xMemCopy(dst, (const void __xdata*)ptr, len);
	dst += len;

	if (padWithXonConstant) {
		
		uint8_t ctr = 0;
		
		while (sendLen--) {
			uint8_t tx = 0;
			
			if (++ctr == 2)
				tx = 0xff;
			else if (ctr == 10)
				ctr = 0;
			*dst++ = tx;
		}
	}
	else
		xMemSet(dst, 0, sendLen);
	
	//apply corrections
	if (corrections) {
		uint16_t ofst;
		
		for (; (ofst = corrections->offset) != 0xffff; corrections++) {
			
			struct LutCorrectionRange __code *rng;
			uint8_t i, len = corrections->len;
			
			//result guaranteed to be found by our range requirements
			for (rng = corrections->ranges; mCurTemperature < rng->minTemp || mCurTemperature > rng->maxTemp; rng++);
			
			for (i = 0; i < len; i++)
				mScreenRow[ofst++] = rng->vals[i];
		}
	}
	einkSelect();
	screenPrvSendCommand(cmd);
	dst = mScreenRow;
	while (origSendLen--)
		screenPrvSendData(*dst++);
	einkDeselect();
}

void P1INT_ISR(void) __interrupt (15)
{
	SLEEP &= (uint8_t)~(3 << 0);	//wake up
}

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

static void screenInitIfNeeded(__bit forPartial)
{
	if (mInited)
		return;
	
	mInited = true;
	
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
	
	//we can now talk to it
	
	einkSelect();
	screenPrvSendCommand(0x06);
	screenPrvSendData(0x0e);		//as per datasheet
	screenPrvSendData(0xcd);
	screenPrvSendData(0x26);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x01);
	screenPrvSendData(0x07);
	screenPrvSendData(0x00);
	screenPrvSendData(0x02);
	screenPrvSendData(0x02);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x04);
	einkDeselect();
	
	//wait for not busy
	while (!(P1 & (1 << 0)));
	
	einkSelect();
	screenPrvSendCommand(0x65);
	screenPrvSendData(0x00);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x00);
	screenPrvSendData(0x8f);
	screenPrvSendData(0x80);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x30);
	screenPrvSendData(0x3a);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x61);
	screenPrvSendData(SCREEN_WIDTH >> 8);
	screenPrvSendData(SCREEN_WIDTH & 0xff);
	screenPrvSendData(SCREEN_HEIGHT >> 8);
	screenPrvSendData(SCREEN_HEIGHT & 0xff);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x82);
	screenPrvSendData(mScreenVcom);
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x50);
	screenPrvSendData(forPartial ? 0xf7 : 0x77);
	einkDeselect();
	
	if (forPartial) {	//partial-fast LUTs. be cautious to not over-DC the screen
		
		SET_LUT(0x20, mLutFastVcom, 220, 0);
		SET_XON_LUT(mLutFastXon);
		SET_COLOR_LUT(000, mLutFastB, 0);
		SET_COLOR_LUT(001, mLutFastW, 0);
		SET_COLOR_LUT(010, mLutFastNoChange, 0);
		SET_COLOR_LUT(011, mLutFastNoChange, 0);
		SET_COLOR_LUT(100, mLutFastNoChange, 0);
		SET_COLOR_LUT(101, mLutFastNoChange, 0);
		SET_COLOR_LUT(110, mLutFastNoChange, 0);
		SET_COLOR_LUT(111, mLutFastNoChange, 0);
	}
	else {				//full refersh LUTs
		
		SET_LUT(0x20, mLutVcom, 220, 0);
		SET_XON_LUT(mLutXon);
		SET_COLOR_LUT(000, mLutB, 0);
		SET_COLOR_LUT(001, mLutG0, 0);
		SET_COLOR_LUT(010, mLutG1, 0);
		SET_COLOR_LUT(011, mLutG2, 0);
		SET_COLOR_LUT(100, mLutG3, 0);
		SET_COLOR_LUT(101, mLutG4, 0);
		SET_COLOR_LUT(110, mLutW, 0);
		SET_COLOR_LUT(111, mLutBrightYellow, corrYellowStageTwo);
	}
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
	uint16_t r, c;
	
	screenInitIfNeeded(false);
	
	einkSelect();
	screenPrvSendCommand(0x10);
		
	for (r = 0; r < SCREEN_HEIGHT; r++) {
		
		uint8_t prevVal = 0;
		
		for (c = 0; c < SCREEN_WIDTH; c++) {
			
			uint8_t val = ((r - c) >> 4) & 7;
			
			if ((uint8_t)c & 1)
				screenPrvSendData((prevVal << 4) | val);
			else
				prevVal = val;
		}
	}

	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x12);
	einkDeselect();
	
	screenPrvSleepTillDone();
}


__bit screenTxStart(__bit forPartial)
{
	screenInitIfNeeded(forPartial);
	
	einkSelect();
	screenPrvSendCommand(0x10);
	
	return true;
}

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte)
{
	screenPrvSendData(byte);
}

void screenTxEnd(void)
{
	einkDeselect();
	
	einkSelect();
	screenPrvSendCommand(0x12);
	einkDeselect();
	
	timerDelay(TIMER_TICKS_PER_SECOND / 10);
	
	screenPrvSleepTillDone();
	
	screenShutdown();
}

