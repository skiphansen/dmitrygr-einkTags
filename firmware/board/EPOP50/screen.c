#include "screen.h"
#include "printf.h"
#include "cpu.h"
#include "timer.h"
#include "adc.h"


/*
	//LCD pinout:
	
	//	WAKE		= P0.7		Set to zero in init func, set to 1 to wake it
	//	nRESET		= P1.2
	//	READY		= P1.0		//only used for wake, not for redraw it seems
	//	nCS			= P1.1
	//	SCK			= P1.3
	//	MISO		= P1.4
	//	MOSI		= P1.5
	
	//display is big endian when using 16bit values in commands/responses
	
	//spi protocol is easy. each register has a 7-bit address
	//first spi byte sent by master determines the register accessed and the mode
	//top bit set means read, top bit cleared means write
	//following bytes read/write the register. in case of read, send zeroes
	//in case of write, ignore read values
	//registers all have a size, and they differ in size. writing past the end or reading past the end is undefined. somietimes it wraps, sometimes it does not
	
	//REGISTERS:
	//	address		name		size	mode	use & notes
	//	0x00		REFRESH		0		W		write to cause a refresh to start
	//	0x01		SCR_H		1		RW		screen height. defaults to full height. changing it causes driver to only drive this much
	//	0x02		SCR_W		1		RW		screen width. defaults to full width. changing it causes driver to only drive this much
	//	0x03		SCP_FLIP	1		RW		bitmask. 0x01 = flip vertical. 0x02 - flip horizontal.
	//	0x04		SCR_SHIFT	2		RW		top byte is "scroll x" bottom is "scroll y" - scrolls the video buffer
	//	0x05		DATA		2520	RW		data for screen. readable
	//	0x07		FW_VER		2		R		fw checks that it is not 0xffff, else no display
	//	0x08		TEMP		1		RW		needs to be written to current temperature in degrees C for proper refresh timing
	//	0x0a		PROTO_VER	2		R		fw checks that this is 0x0001, else no display
	//	0x0b		STATUS		1		R		status bits. bit 0x80 is high while refresh is ongoing. bit 0x40 is high while op EEREAD is ongoing
	//	0x0c		REFR_SPEC	5		RW		part of precise refresh spec. firmware writes this is base sends it what to write. leave it alone for reset results
	//	0x0d		TEST_IMG	1		W		write to show test image on next refresh. bottom 2 bits is image type: 00 = black, 01 and 03 = white, 02 = special. for special, next 3 bits determins function. 0..6 are sizes for a checkerboard pattern, 7 is vertical stripes one pixel wide
	//	0x0e		UNK1		2		W		firmware writes it to SCREEN_HEIGHT. seems to make no difference
	//	0x10		NUM_DRAWS	4		R		number of screen refreshes so far. writes are ignored
	//	0x11		UNK2		1		RW		part of precise refresh spec. firmware writes this is base sends it what to write. leave it alone for reset results. defaults to 0x94
	//	0x14		SHUTDOWN	0		W		write to power the display off
	//	0x15		UNK3		1		RW		part of precise refresh spec. firmware writes this is base sends it what to write. leave it alone for reset results. defaults to 0x0b
	//	0x16		UNK4		1		RW		part of precise refresh spec. firmware writes this is base sends it what to write. leave it alone for reset results. defaults to 0x01
	//	0x17		UNK5		1		RW		part of precise refresh spec. firmware writes this is base sends it what to write. leave it alone for reset results. defaults to 0x3f
	//	0x1a		EEREAD		0		W		instruct display to write its internal eeprom to VRAM,  after this we wait for reply from STATUS bits 0x40 to go low when done. vram can then be read
	//	0x1d		UNK6		6		RW		written to these exact bytes by firmware: a5 80 00 f1 4c 07 08. we do same. default is a5 80 04 71 4c 07 08

*/

__xdata __at (0xfda2) uint8_t mScreenRow[320];	//350 bytes here, we use 320 of them


static __bit mInited = false;


#pragma callee_saves screenPrvWaitByteSent
static uint8_t screenSendByteAndWaitTxDone(uint8_t byte)
{
	U0DBUF = byte;
	while (!(U0CSR & 0x02));
	U0CSR &= (uint8_t)~0x02;
	return U0DBUF;
}

#define screenPrvSelect()			\
	do {							\
		P1 &= (uint8_t)~(1 << 1);	\
	} while (0)

#define screenPrvDeselect()			\
	do {							\
		P1 |= (uint8_t)(1 << 1);	\
	} while (0)

static uint16_t screenPrvRead16(uint8_t cmd)
{
	uint16_t ret;
	
	screenPrvSelect();
	screenSendByteAndWaitTxDone(cmd);
	ret = screenSendByteAndWaitTxDone(0);
	ret <<= 8;
	ret |= screenSendByteAndWaitTxDone(0);
	screenPrvDeselect();
	
	return ret;
}

static uint8_t screenPrvRead8(uint8_t cmd)
{
	uint8_t ret;
	
	screenPrvSelect();
	screenSendByteAndWaitTxDone(cmd);
	ret = screenSendByteAndWaitTxDone(0);
	screenPrvDeselect();
	
	return ret;
}

static void screenPrvSimpleCommand(uint8_t cmd)
{
	screenPrvSelect();
	screenSendByteAndWaitTxDone(cmd);
	screenPrvDeselect();
}

static void screenPrvWrite8(uint8_t cmd, uint8_t val)
{
	screenPrvSelect();
	screenSendByteAndWaitTxDone(cmd);
	screenSendByteAndWaitTxDone(val);
	screenPrvDeselect();
}

static void screenPrvWrite16(uint8_t cmd, uint16_t val)
{
	screenPrvSelect();
	screenSendByteAndWaitTxDone(cmd);
	screenSendByteAndWaitTxDone(val >> 8);
	screenSendByteAndWaitTxDone(val);
	screenPrvDeselect();
}

static void screenPrvIfaceDeinit(void)
{
	P0 &= (uint8_t)~(1 << 7);
	
	U0CSR = 0;
	P1SEL &= (uint8_t)~((1 << 3) | (1 << 4) | (1 << 5));
}

static __bit screenInitIfNeeded(void)
{
	uint8_t retries, vW, vH;
	uint16_t val16;
	
	if (mInited)
		return true;
	
	//configure pins
	
	//default state set (incl keeping it in reset and disabled)
	P0 &= (uint8_t)~(1 << 7);
	P1 = (P1 & (uint8_t)~(1 << 2)) | (1 << 1);
	
	//pins are gpio
	P0SEL |= (uint8_t)(1 << 7);
	P1SEL = (P1SEL & (uint8_t)~((1 << 0) | (1 << 1) | (1 << 2))) | (1 << 3) | (1 << 4) | (1 << 5);
	
	//directions set as needed
	P0DIR |= (uint8_t)(1 << 7);
	P1DIR = (P1DIR & (uint8_t)~((1 << 0) | (1 << 4))) | (1 << 1) | (1 << 2) | (1 << 3) | (1 << 5);

	//configure the uart0 (alt2, spi, fast)
	PERCFG |= (1 << 0);
	U0BAUD = 0;			//F/8 is max for spi - 3.25 MHz
	U0GCR = 0b00110001;	//BAUD_E = 0x11, msb first
	U0CSR = 0b01000000;	//SPI master mode
	
	P2SEL &= (uint8_t)~(1 << 6);
	
	P0 |= (1 << 7);
	
	//reset the screen
	P1 &= (uint8_t)~(1 << 2);
	timerDelay(TIMER_TICKS_PER_SECOND / 1000);	//wait an msec
	P1 |= (1 << 2);
	
	//wait for it to be awake
	retries = 200;
	while (--retries && !(P1 & (1 << 0)))
		timerDelay(TIMER_TICKS_PER_SECOND / 1000);
	
	if (!retries) {
		pr("display failed to wake up\n");
		goto fail;
	}
	
	val16 = screenPrvRead16(0x8a);
	if (val16 != 0x0001) {
		pr("display reg 0x0a val unexpected: 0x%04x\n", val16);
		goto fail;
	}
	
	val16 = screenPrvRead16(0x87);
	if (val16 == 0xffff) {
		pr("display reg 0x07 val unexpected: 0x%04x\n", val16);
		goto fail;
	}
	
	vW = screenPrvRead8(0x82);
	vH = screenPrvRead8(0x81);
	if (vW != SCREEN_WIDTH || vH != SCREEN_HEIGHT) {
		
		pr("screen dimensions %u x %u not as expected\n", vW, vH);
		goto fail;
	}

	//magic...
	screenPrvSelect();
	screenSendByteAndWaitTxDone(0x1d);
	screenSendByteAndWaitTxDone(0xff);	//read value is 0xa5	some sort of a speed thing. lower values seem to do faster refresh, down to 0
	screenSendByteAndWaitTxDone(0x80);	//read value is 0x80
	screenSendByteAndWaitTxDone(0x04);	//read value is 0x04, bit 0x10 disables all redraw
	screenSendByteAndWaitTxDone(0xf1);	//read value is 0x71, bit 0x02 maeks all black, others seemingly no effect
	screenSendByteAndWaitTxDone(0x4c);	//read value is 0x4c	voltage for contrast, etc
	screenSendByteAndWaitTxDone(0x07);	//read value is 0x07
	screenSendByteAndWaitTxDone(0x08);	//read value is 0x08
	screenPrvDeselect();
	
	mInited = true;
	return true;

fail:
	screenPrvIfaceDeinit();
	return false;
}

void screenShutdown(void)
{
	if (!mInited)
		return;
	
	screenPrvSimpleCommand(0x14);
	screenPrvIfaceDeinit();
	
	mInited = false;
}

/*	//comment out cause SDCC else wastes ram and flash on it

	//this routine will corrupt screen settings (regs1..4) and VRAM
	void screenPrvGetSerialNumber(char __xdata *dst)
	{
		uint8_t j;
		
		dst[0] = 0;
		
		if (!screenInitIfNeeded()) {
			pr("init screen..FAIL\n");
			return;
		}
		
		screenPrvWrite8(0x01, 96);
		screenPrvWrite8(0x02, 240);
		screenPrvWrite8(0x03, 0);
		screenPrvWrite16(0x04, 0);
		screenPrvSimpleCommand(0x1a);
		while (screenPrvRead8(0x8b) & 0x40);
		
		screenPrvWrite16(4, 0x6010);
		
		screenPrvSelect();
		screenSendByteAndWaitTxDone(0x85);
		//skip one dummy byte on the read
		screenSendByteAndWaitTxDone(0);
		
		for (j = 0; j < 19; j++)
			dst[j] = screenSendByteAndWaitTxDone(0);
		dst[j] = 0;
		screenPrvDeselect();
		
		if (dst[0] == 0xff)
			dst[0] = 0;
	}
*/

void screenTest(void)
{
	uint8_t i, j;
	
	screenTxStart(false);
	for (i = 0; i < SCREEN_HEIGHT; i++) {
		
		for (j = 0; j < (SCREEN_WIDTH + 7) / 8; j++)
			screenByteTx(0xf0 >> (i & 4));
	}
	screenTxEnd();
	
	pr("deinit screen..DONE\n");
}

__bit screenTxStart(__bit forPartial)
{
	if (forPartial)
		return false;
	
	if (!screenInitIfNeeded())
		return false;
	
	screenPrvSelect();
	screenSendByteAndWaitTxDone(0x05);
	
	return true;
}

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte)
{
	screenSendByteAndWaitTxDone(~byte);	//keep black as 0 for convenience...
}

void screenTxEnd(void)
{
	if (!mInited)
		return;
	
	screenPrvDeselect();
	
	screenPrvWrite8(0x08, adcSampleTemperature());
	
	screenPrvSimpleCommand(0x00);
	while (screenPrvRead8(0x8b) & 0x80);
	
	/*
	screenPrvSelect();
	screenSendByteAndWaitTxDone(0x90);
	pr("REFRESH CT: ");
	for (i = 0; i < 4; i++)
		pr(" %02x", screenSendByteAndWaitTxDone(0));
	pr("\n");
	screenPrvDeselect();
	*/
	
	screenShutdown();
}

