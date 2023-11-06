#include "screen.h"
#include "printf.h"
#include "cc111x.h"
#include "timer.h"
#include "adc.h"


__xdata __at (0xfda2) uint8_t mScreenRow[320];	//350 bytes here, we use 320 of them

/*
	pinout (as per how rom does it)
	
	digital:
		P0 [all of it]: data
		P1.0	out		pixel clock
		P1.1	out		lcd supply 1	(toggled with power states)
		P1.2	out		lcd supply 2	(just set once, maybe not needed)
		P1.5	out		row advance
		P1.6	out		erase
		P1.7	out		row latch
		P2.1	out		enable
		P2.2	out		frame marker
	analog out (pwm + filter):
		P1.3	
		P1.4
		
	to send a byte of data to the screen:		//procedure: BYTE
		1. data out on Port0
		2. pixel clock pin up
		3. small delay (9 nops)
		4. pixel clock line down
		5. small delay (9 nops)
	
	to send a LINE of data to the screen:		//procedure: LINE
		1. call BYTE() 5 bytes (data is do not care)
		2. call BYTE() ceil(SCREEN_WIDTH / 8) times, with the line data
		3. row latch line up
		4. small delay (9 nops)
		5. row latch line down
		6. if this is the first line, goto 11, else:
		7. row advance line up
		8. small delay (9 nops)
		9. row advance line down
		10. if frame marker is high, lower it
		10. delay based on current temperature (in room temp, around 10ms)
		11. frame marker low
		12. goto 10
	
	to send a screenful:						//procedure: SCREEN
		1. erase pin low
		2. enable pin high
		3. frame marker high
		4. call LINE() SCREEN_HEIGHT times
		5. enable pin low
	
	to clean screen:							//procedure: CLEAN
		1. erase pin high
		2. enable pin high
		3. frame marker high
		4. call LINE() once, all data should be 0xff. consider this "first line"
			this same loaded data will be reused for each row, no need to reload it, but it is safe to reload it if needed
			if not 0xff is used for data, the bits set to zero will not be erased. also, if new data is loaded each row,
			it is possible to erase carefully-selected pixels
		5. (SCREEN_HEIGHT + 8) x times do:
			a. delay based on current temperature (in room temp, around 10ms)
			b. row advance line up
			c. small delay (9 nops)
			d. row advance line down
			e. if this is the first time,  frame marker low
		6. enable linw low
		7. wait a while (a second or so)
	
	OR, clean screen can simply be a draw procedure with eras epin high (it is the same afterral)
	
	
	
*/


static uint8_t __xdata mScreenColCtr, mOldP1sel;
static __bit mScreenRowFirst;



#define EPD_PIX_CLK			P1_0
#define EPD_SUPPLY_1		P1_1
#define EPD_SUPPLY_2		P1_2
#define EPD_ROW_ADVANCE		P1_5
#define EPD_ERASE			P1_6
#define EPD_ROW_LATCH		P1_7
#define EPD_ENABLE			P2_1
#define EPD_FRAME_MARK		P2_2

#pragma callee_saves screenPrvSendByte
static void epdDelay(void) __reentrant __naked
{
	__asm__(
		"	mov b, #20			\n"
		"00001$:				\n"
		"	djnz b, 00001$		\n"
		"	ret					\n"
	);
}

static void screenPrvSetAnalogVoltages(uint8_t pwm0, uint8_t pwm1)
{
	//set the new levels
	T3CC0 = pwm0;
	T3CC1 = pwm1;
	
	//get the capacitors time to reach the new levels
	timerDelay(TIMER_TICKS_PER_SECOND / 100);
}

void screenTest(void)
{
	uint16_t row;
	uint8_t byte;


	screenTxStart(false);
	for (row = 0; row < SCREEN_HEIGHT; row++) {
		for (byte = 0; byte < (SCREEN_WIDTH + 7) / 8; byte++) {
			
			screenByteTx(((row >> 3) ^ byte) & 1 ? 0xff : 0x00);
		}
		pr("\n");
	}
	
	screenTxEnd();

	pr("done\n");
}

#pragma callee_saves screenPrvSendByte
static void screenPrvSendByte(uint8_t byte)
{
	P0 = byte;
	EPD_PIX_CLK = true;
	epdDelay();
	EPD_PIX_CLK = false;
	epdDelay();
}

__bit screenTxStart(__bit forPartial)
{
	uint8_t __xdata oldP0sel, oldP0dir;
	uint16_t j;
	uint8_t i;
	
	if (forPartial)
		return false;
	
	mScreenColCtr = 0;
	mScreenRowFirst = true;
	
	//init pins
	oldP0sel = P0SEL;
	oldP0dir = P0DIR;
	mOldP1sel = P1SEL;
	P1SEL = (1 << 3) | (1 << 4);
	P2SEL |= 0x20;
	P0SEL = 0;
	P0DIR = 0xff;
	P1DIR = 0xff;
	P2DIR |= (1 << 1) | (1 << 2);
	
	//everything off
	P1 = 0x00;
	P2 &= (uint8_t)~((1 << 1) | (1 << 2));
	
	//set up analog outs
	T3CTL = 0;
	T3CC0 = 0x10;	//gotta start somewhere
	T3CC1 = 0x10;
	T3CCTL1 = 0x1c;
	T3CCTL0 = 0x1c;
	PERCFG &= (uint8_t)~0x20;
	T3CTL = 0x10;
	
	//set voltages for erase
	screenPrvSetAnalogVoltages(0x70, 0xf1);
	
	//power it on
	EPD_SUPPLY_1 = true;
	EPD_SUPPLY_2 = true;
	timerDelay(TIMER_TICKS_PER_SECOND / 100);
	
	//erase it
	EPD_ERASE = true;
	EPD_ENABLE = true;
	epdDelay();
	EPD_FRAME_MARK = true;
	epdDelay();
	for (i = 0; i < (SCREEN_WIDTH + 7) / 8 + 5; i++)
		screenPrvSendByte(0xff);
	epdDelay();
	EPD_ROW_LATCH = true;
	epdDelay();
	EPD_ROW_LATCH = false;
	epdDelay();
	
	for (j = 0; j < SCREEN_HEIGHT + 8; j++) {
		timerDelay(TIMER_TICKS_PER_SECOND / 200);
		
		EPD_ROW_ADVANCE = true;
		epdDelay();
		EPD_ROW_ADVANCE = false;
		epdDelay();
		
		//checking every cycle for whether this is first takes more tmie - just clear it every time - much faster
		EPD_FRAME_MARK = false;
	}
	
	EPD_ENABLE = false;
	timerDelay(TIMER_TICKS_PER_SECOND);
	
	//set voltages for draw
	screenPrvSetAnalogVoltages(0xa0, 0x80);
	
	//prepare for draw
	EPD_FRAME_MARK = true;
	epdDelay();
	EPD_ERASE = false;
	epdDelay();
	EPD_ENABLE = true;
	epdDelay();
	
	P0SEL = oldP0sel;
	P0DIR = oldP0dir;
	return true;
}

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte)
{
	uint8_t __xdata oldP0sel, oldP0dir;
	
	oldP0sel = P0SEL;
	oldP0dir = P0DIR;
	
	P0SEL = 0;		//all gpio
	P0DIR = 0xff;
	
	if (!mScreenColCtr) {
		
		screenPrvSendByte(0);
		screenPrvSendByte(0);
		screenPrvSendByte(0);
		screenPrvSendByte(0);
		screenPrvSendByte(0);
	}
	screenPrvSendByte(~byte);
	mScreenColCtr++;
	if (mScreenColCtr == (SCREEN_WIDTH + 7) / 8) {	//line ended?
		
		mScreenColCtr = 0;
		EPD_ROW_LATCH = true;
		epdDelay();
		EPD_ROW_LATCH = false;
		epdDelay();
		
		if (mScreenRowFirst)
			mScreenRowFirst = false;
		else {
			
			EPD_ROW_ADVANCE = true;
			epdDelay();
			EPD_ROW_ADVANCE = false;
			epdDelay();
			EPD_FRAME_MARK = false;
		}
		
		timerDelay(TIMER_TICKS_PER_SECOND / 500);
	}
	
	P0SEL = oldP0sel;
	P0DIR = oldP0dir;
}

void screenTxEnd(void)
{
	EPD_ENABLE = false;
	timerDelay(TIMER_TICKS_PER_SECOND / 10);
	
	EPD_SUPPLY_1 = false;
	EPD_SUPPLY_2 = false;
	
	P1SEL = mOldP1sel;
}

void screenShutdown(void)
{
	
}