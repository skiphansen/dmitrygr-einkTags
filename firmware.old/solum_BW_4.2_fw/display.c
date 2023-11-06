#include "display.h"
#include <stdint.h>
#include "timer.h"
#include "heap.h"
#include "fw.h"

#if DISPLAY_BPP == 1
	
	#define USE_BUILTIN_LOGIC			//1BPP, temp-based profiles

#elif DISPLAY_BPP == 2
										//2 BPP, room temp only
#else
	#error "Display BPP not supported"
#endif


/*
	Out luts, with some luck, experimentation, and the IL0376F datasheet :)

	struct LutCommand {
		uint8_t cmd;
		struct LutEntry {
			uint8_t numFrames1	: 6;
			uint8_t level1		: 2;
			uint8_t numFrames2	: 6;
			uint8_t level2		: 2;
			uint8_t numRepeats;
		} entries[5];
	};
	
	enum LutLevelSelect {
		LutLevelDc = 0,
		LutLevelHigh = 1,
		LutLevelLow = 2,
		LutLevelFloating = 3,
	};

*/

#define GPIO_NUM_HV_SUPPLY_nENABLE		2
#define GPIO_NUM_LEFT_DnC				23
#define GPIO_NUM_nRESET					24
#define GPIO_NUM_BUSY					25
#define GPIO_NUM_RIGHT_DnC				27

//26 - out
//5 - in


static const uint8_t mNormalLuts[] = {

	0x20,					//VCOM
		0x08, 0x1C, 0x01,
		0x19, 0x19, 0x05,
		0x0C, 0x02, 0x02,
		0x02, 0x03, 0x01,
		0x02, 0x05, 0x01,
	
	0x21,					//W
		0x48, 0x5C, 0x01,	// 8hi,28hi
		0x99, 0x59, 0x05,	//25lo,25hi
		0x8C, 0x82, 0x02,	//12lo,2lo
		0x02, 0x83, 0x01,	// 2dc,3lo
		0x02, 0x85, 0x01,	// 2dc,5lo
	
	0x24,					//Light Gray
		0x04, 0x5c, 0x01,	// 4dc,28hi
		0x99, 0x59, 0x05,	//25lo,25hi
		0x0c, 0x82, 0x02,	//12dc,2lo
		0x03, 0x82, 0x01,	// 3dc,2lo
		0x05, 0x82, 0x01,	// 5dc,2lo
	
	0x23,					//Gark Gray
		0x0a, 0x5a, 0x01,	//10dc,22hi
		0x99, 0x59, 0x05,	//25lo,25hi
		0x0c, 0x82, 0x02,	//12dc,2lo
		0x04, 0x41, 0x01,	// 4dc,1hi
		0x06, 0x01, 0x01,	// 6dc,1dc
	
	0x22,					//B
		0x08, 0x9C, 0x01,	// 8dc,28lo
		0x99, 0x59, 0x05,	//25lo,25hi
		0x4C, 0x02, 0x02,	//12hi, 2dc
		0x42, 0x03, 0x01,	// 2hi, 3dc
		0x42, 0x05, 0x01,	// 2hi,	5dc
};

static const uint8_t mPartialLuts[] = {

	0x20,					//VCOM
		0x04, 0x01, 0x04,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
	
	0x21,					//idx0: B->W
		0x84, 0x01, 0x04,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
	
	0x24,					//idx1: W->B
		0x44, 0x01, 0x04,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
	
	0x23,					//idx2: keep
		0x04, 0x01, 0x04,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
	
	0x22,					//idx3: keep
		0x04, 0x01, 0x04,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x00, 0x00, 0x00,
};



static void *mFramebuffer;

void displayInit(void)
{
	mFramebuffer = heapAlloc(((DISPLAY_WIDTH * DISPLAY_BPP + 7) / 8) * DISPLAY_HEIGHT);
	
	//start out with a low power consumption :)
	gpioSet(GPIO_NUM_HV_SUPPLY_nENABLE, 1);
}

void *displayGetFbPtr(void)
{
	return mFramebuffer;
}

static bool displayPrvWait(bool desiredState)
{
	uint64_t time = timerGet();

	while (!gpioGet(GPIO_NUM_BUSY) != !desiredState) {
		if (timerGet() - time >= 5 * TIMER_TICKS_PER_SEC)
			return false;
		
		wdtPet();
	}
	
	return true;
}

static void einkCompleteCmd(const uint8_t *ptr, uint32_t numDataBytes)
{
	einkLlSendCommand(*ptr++);
	while(numDataBytes--)
		einkLlSendData(*ptr++);
}

static bool einkInit(void)
{
	static const uint8_t cmdPowerSettings[] = {0x01, 0x03, 0x00, 0x00, 0x00};
	static const uint8_t cmdBoosterStart[] = {0x06, 0x07, 0x06, 0x05};
	static const uint8_t cmdDataIntervalSetting[] = {0x50, 0x07};
	static const uint8_t cmdPllSetting[] = {0x30, 0x31};
	static const uint8_t cmdResoluton[] = {0x61, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT >> 8, (uint8_t)DISPLAY_HEIGHT};
	static const uint8_t cmdVcomSetting[] = {0x82, 0x04};
	static const uint8_t cmdPannelSetting[] = {0x00, 0xdf};
	
	//patch out config for some interrupt stuff
	*(volatile uint32_t*)0x201044D6 = 0x20012001;
	
	einkInterfaceInit();
	
	//reset it
	gpioSet(GPIO_NUM_nRESET, 0);
	timerDelay(TIMER_TICKS_PER_MSEC / 10);
	gpioSet(GPIO_NUM_nRESET, 1);
	timerDelay(TIMER_TICKS_PER_MSEC);
	wdtPet();
	
	if (!displayPrvWait(false))
		return false;
	
	gpioSet(GPIO_NUM_HV_SUPPLY_nENABLE, 0);
	
	einkCompleteCmd(cmdPowerSettings, sizeof(cmdPowerSettings) - 1);
	einkCompleteCmd(cmdBoosterStart, sizeof(cmdBoosterStart) - 1);
	
	einkLlSendCommand(0x04);	//start
	
	if (!displayPrvWait(true))
		return false;
	
	einkCompleteCmd(cmdDataIntervalSetting, sizeof(cmdDataIntervalSetting) - 1);
	einkCompleteCmd(cmdPllSetting, sizeof(cmdPllSetting) - 1);
	einkCompleteCmd(cmdResoluton, sizeof(cmdResoluton) - 1);
	einkCompleteCmd(cmdVcomSetting, sizeof(cmdVcomSetting) - 1);
	einkCompleteCmd(cmdPannelSetting, sizeof(cmdPannelSetting) - 1);
	
	return true;
}

bool TEXT2 displayRefresh(bool partial)
{
	static const uint8_t expandTab[] = {0x00, 0xc0, 0x30, 0xf0, 0x0c, 0xcc, 0x3c, 0xfc, 0x03, 0xc3, 0x33, 0xf3, 0x0f, 0xcf, 0x3f, 0xff};
	uint32_t i, j, k, bpp, temp, einkTempRange;
	const uint8_t *data, *lutData;
	bool ret = false;
	
	adcConfigTempMeas();
		
	//adcMeasure
	temp = adcDoMeasure();
	
	//adcTurnOff
	adcOff();
	
	einkTempRange = einkLlTempToCalibRange(temp);
	
	#ifdef USE_BUILTIN_LOGIC
		
		bpp = 1;
		
		lutData = gEinkLuts[einkTempRange];
	
	#else
		
		lutData = partial ? mPartialLuts : mNormalLuts;
		bpp = 2;
		
	#endif
	
	*(volatile uint8_t*)0x20104620 = 0x05;	//patch bug in calibration loading code (loads 8 rows instead of 5)
	
	if (!einkInit())
		return false;
	
	einkLlSendCommand(0x04);
	
	if (!displayPrvWait(true))
		goto out;
	
	einkLlSendTempCalib(lutData);
	
	/*
		screen acts like two separate screens 200x300 screens stacked next to each other.
		we act as is we have one screebuffer to save headache...
	
	*/
	
	for (i = 0; i < 2; i++) {
	
		data = i ? (mFramebuffer + DISPLAY_WIDTH * bpp / 2 / 8) : mFramebuffer;
		
		einkLlSendCommand(0x10);
	
		gpioSet(i ? GPIO_NUM_RIGHT_DnC : GPIO_NUM_LEFT_DnC, 0);
		
		for (j = 0; j < DISPLAY_HEIGHT; j++) {
			
			for (k = 0; k < DISPLAY_WIDTH / 2; k += 8 / bpp) {
				uint32_t byte = *data++;
				
				if (bpp == 1) {
					//sspTx 
					einkLlRawSpiByte(1, expandTab[byte & 0x0f]);
					
					//sspTx
					einkLlRawSpiByte(1, expandTab[byte >> 4]);
				}
				else if (bpp == 2) {
					
					//swap groups of 2 bits
					byte = ((byte & 0xf0) >> 4) | ((byte << 4) & 0xf0);
					byte = ((byte & 0xcc) >> 2) | ((byte << 2) & 0xcc);
					
					einkLlRawSpiByte(1, byte);
				}
			}
			data += DISPLAY_WIDTH * bpp / 2 / 8;
			wdtPet();
		}
		
		gpioSet(i ? GPIO_NUM_RIGHT_DnC : GPIO_NUM_LEFT_DnC, 1);
	}
	
	einkLlSendCommand(0xe0);
	einkLlSendData(0x01);
	
	einkLlSendCommand(0x12);
	
	if (!displayPrvWait(true))
		goto out;
	
	einkLlSendCommand(0x02);
	
	if (!displayPrvWait(false))
		goto out;
	
	ret = true;
out:
	gpioSet(GPIO_NUM_HV_SUPPLY_nENABLE, 1);
	return ret;
}


void dispTest(void)
{
	uint8_t *buf = mFramebuffer;
	uint32_t r, c, pos, color;
	
	for (r = 0; r < 300; r++) {
		
		for (c = 0; c < 400; c++) {
			
			pos = r * 400 + c;
			
			color = ((r / 8) + (c / 8)) & 3;
			
			buf[pos / 4] &=~ (3 << ((pos % 4) * 2));
			buf[pos / 4] |= color << ((pos % 4) * 2);
		}
		wdtPet();
	}
	displayRefresh(false);
	
	while(1);
}