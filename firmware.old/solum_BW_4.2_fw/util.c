#include <stdarg.h>
#include <stdio.h>
#include "eeprom.h"
#include "timer.h"
#include "mz100.h"
#include "util.h"
#include "fw.h"




static uint64_t mRndState[2];

uint32_t rnd32(void)//xorshift128plus
{
	unsigned char *state = (unsigned char*)mRndState;
	uint64_t ret, x, y;
	
	//seed cannot be zero - make it one
	if (!mRndState[0] && !mRndState[1])
		mRndState[0] = 1;
	
	x = mRndState[0];
	y = mRndState[1];
	
	mRndState[0] = y;
	x ^= x << 23; // a
	mRndState[1] = x ^ y ^ (x >> 17) ^ (y >> 26); // b, c
	ret = mRndState[1] + y;

	
	//ret is the 64-bit random number generated
	
	return ((uint32_t)(ret >> 32)) ^ (uint32_t)ret;
}

uint32_t measureTemp(void)
{
	uint32_t i, t = 0;
	
	adcConfigTempMeas();
	
	//warm up
	for (i = 0; i < 32; i++)
		adcDoMeasure();
	
	for (i = 0; i < 128; i++)
		t += adcDoMeasure();
	
	adcOff();
	
	return t;
}

uint32_t measureBattery(void)
{
	uint32_t oldWord, ret;
	
	//patch battery measurement to return raw ADC value
	oldWord = *(volatile uint32_t*)0x201004f0;
	*(volatile uint32_t*)0x201004F0 = 0x81f0e8bd;
	
	ret = fwBatteryRawMeasure();
	
	*(volatile uint32_t*)0x201004F0 = oldWord;
	
	//this is broken on our development board but works on real tags!
	//result is 32768 * volts / 3.6
	
	return ret * 3600 / 32768;
}

int snprintf(char *s, size_t n, const char *format, ... )
{
	va_list vl;
	int ret;
	
	va_start(vl, format);
	ret = vsnprintf(s, n, format, vl);
	va_end(vl);
	
	return ret;
}

int sprintf(char *s, const char *format, ... )
{
	va_list vl;
	int ret;
	
	va_start(vl, format);
	ret = vsnprintf(s, 0x7fffffff, format, vl);
	va_end(vl);
	
	return ret;
}

//patches existing erase code - this is smaller
static bool qspiEraseWithCmdAndDirectAddr(uint32_t at, uint_fast8_t cmd, uint32_t timeout)
{
	uint32_t oldTimeout;
	bool ret;
	
	//patch QSPI command
	*(volatile uint8_t*)0x20100ee8 = cmd;
	
	//patch left shift
	*(volatile uint16_t*)0x20100ed0 = 0xbf00;
	
	//patch overflow check
	*(volatile uint16_t*)0x20100ec8 = 0xbf00;
	
	//patch timeout (save as it is used by writing func too
	oldTimeout = *(volatile uint32_t*)0x20101070;
	*(volatile uint32_t*)0x20101070 = timeout;
	
	ret = qspiEraseSector(at);
	
	*(volatile uint32_t*)0x20101070 = oldTimeout;
	
	return ret;
}

static bool qspiErase4K(uint32_t at)
{
	return qspiEraseWithCmdAndDirectAddr(at, 0x20, 1000000);
}

static bool qspiErase32K(uint32_t at)
{
	return qspiEraseWithCmdAndDirectAddr(at, 0x52, 32000000);
}

static bool qspiErase64K(uint32_t at)
{
	return qspiEraseWithCmdAndDirectAddr(at, 0xd8, 64000000);
}

void qspiEraseRange(uint32_t addr, uint32_t len)
{
	uint64_t time;
	
	//round starting address down
	if (addr % EEPROM_PAGE_SIZE) {
		
		len += addr % EEPROM_PAGE_SIZE;
		addr = addr / EEPROM_PAGE_SIZE * EEPROM_PAGE_SIZE;
	}
	
	//round length up
	len = (len + EEPROM_PAGE_SIZE - 1) / EEPROM_PAGE_SIZE * EEPROM_PAGE_SIZE;
	
	while (len) {
		
		uint32_t now;
		bool ok;
		
		wdtPet();
		if (!(addr % 0x10000) && len >= 0x10000) {
			ok = qspiErase64K(addr);
			now = 0x10000;
		}
		else if (!(addr % 0x8000) && len >= 0x8000) {
			ok = qspiErase32K(addr);
			now = 0x8000;
		}
		else {
			ok = qspiErase4K(addr);
			now = 0x1000;
		}
		
		if (!ok)
			pr("ERZ fail at 0x%08x + %u\r\n", addr, now);
		
		addr += now;
		len -= now;
		if (len) {
			//let the caps recharge
			time = timerGet();
			while (timerGet() - time < TIMER_TICKS_PER_SEC / 10);
		}
	}
	wdtPet();
}

void setPowerState(bool fast)
{
	if (fast) {
		PMU->FREQ = 32;
		PMU->CLK_SRC &=~ 1;
	}
	else {
		PMU->CLK_SRC |= 1;
		PMU->FREQ = 1;
	}
}

void radioShutdown(void)
{
	//i have no idea what these do, determined by writing random registers and watching the current drawn
	*(volatile uint32_t*)0x4C000000 = 0;
	*(volatile uint32_t*)0x4C010000 = 0;
	*(volatile uint32_t*)0x4C010004 = 0x10000000;
}