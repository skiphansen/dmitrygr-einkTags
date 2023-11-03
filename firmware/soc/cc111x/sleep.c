#include "sleep.h"
#include "cpu.h"



void WOR_ISR(void) __interrupt (5)
{
	WORIRQ &= (uint8_t)~(1 << 0);
	SLEEP &= (uint8_t)~(3 << 0);
}

void sleepTillInt(void)	//assumes you left only one int enabled!
{
	PCON |= 0x01; // Enter PM2
}

void sleepForMsec(uint32_t units)
{
	uint8_t __xdata PM2_BUF[7] = {0x06,0x06,0x06,0x06,0x06,0x06,0x04};
	struct DmaDescr __xdata dmaDesc = {
		.srcAddrHi = ((uint16_t)PM2_BUF) >> 8,
		.srcAddrLo = (uint8_t)PM2_BUF,
		.dstAddrHi = 0xdf,
		.dstAddrLo = 0xbe,		//SLEEP REG
		.vlen = 0,				//tranfer given number of bytes
		.lenLo = 7,				//exactly 7 bytes
		.trig = 0,				//manual trigger
		.tmode = 1,				//block xfer
		.wordSize = 0,			//byte-by-byte xfer
		.priority = 2,			//higher-than-cpu prio
		.irqmask = 0,			//most definitely do NOT cause an irq
		.dstinc = 0,			//do not increment DST (write SLEEP reg repeatedly)
		.srcinc = 1,			//increment source
	};
	__bit forever = !units;
	uint8_t tmp;
	
	
	
	units >>= 5;	//our units are 31.25msec, caller used msec
	
	//we are now running at 13MHz from the RC osc

	//disable non-wake irqs
	IRCON &= (uint8_t)~(1 << 7);
	IEN0 = (uint8_t)((1 << 5) | (1 << 7));

	while (forever || units) {
	
		uint16_t now;
	
		//sleep mode errata require it to be enteresd only from RCOSC
		CLKCON |= 0x01;				//clock down to clock/2 as that is the best we get from rcosc
		CLKCON |= 0x40;				//switch to rc osc
		while (!(SLEEP & 0x20));	//wait for HFRC to stabilize 
		while (!(CLKCON & 0x40));	//wait for the switch
	
		if (units > 0xfff0) {		//we can sleep for up to 33 mins effectively this way
			now = 0xfff0;
			units -= 0xfff0;
		}
		else {
			now = units;
			units = 0;
		}
		now += 2;	//counter starts at 2 due to how we init it
		
		//enable WOR irq
		WORIRQ = forever ? 0x00 : 0x10;
		IRCON = 0;
		
		DMAARM |= 0x81;
		DMA0CFG = (uint16_t)&dmaDesc;
		DMAARM = 0x01;
		
		// Reset Sleep Timer. 
		WORCTRL = 0x04;
		while (WORTIME0);
		
		WORCTRL = 0x02;		//div2^10 (approx 31.25msec)
		WOREVT1 = now >> 8;
		WOREVT0 = now & 0xff;
		
		// Wait until a positive 32 kHz edge
		tmp = WORTIME0;
		while(tmp == WORTIME0); 
		
		MEMCTR |= 0x02;
		SLEEP = 0x06;
		
		__asm__(
			"	nop 				\n"
			"	nop 				\n"
			"	nop 				\n"
		);
		
		if (SLEEP & 0x03) {
			__asm__ (
				"	mov _DMAREQ, #0x01	\n"
				"	nop					\n"
				"	orl _PCON, #0x01	\n"
				"	nop					\n"
			);
		}
		
		MEMCTR &= (uint8_t)~0x02;
	}

	
}




