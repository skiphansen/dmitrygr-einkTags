#include <stdbool.h>
#include "asmUtil.h"
#include "eeprom.h"
#include "printf.h"
#include "screen.h"
#include "board.h"
#include "adc.h"


#define MAC_SECOND_WORD		0x44674b7aUL


#pragma callee_saves prvReadSetting
static int8_t prvReadSetting(uint8_t type, uint8_t __xdata *dst, uint8_t maxLen)	//returns token length if found, copies at most maxLen. returns -1 if not found
{
	static const uint8_t __xdata magicNum[4] = {0x56, 0x12, 0x09, 0x85};
	uint8_t __xdata tmpBuf[4];
	uint8_t pg, gotLen = 0;
	__bit found = false;
	
	for (pg = 0; pg < 10; pg++) {
		
		uint16_t __pdata addr = mathPrvMul16x8(EEPROM_ERZ_SECTOR_SZ, pg);
		uint16_t __pdata ofst = 4;
		
		eepromRead(addr, tmpBuf, 4);
		
		if (xMemEqual(tmpBuf, magicNum, 4)) {
		
			while (ofst < EEPROM_ERZ_SECTOR_SZ) {
		
				eepromRead(addr + ofst, tmpBuf, 2);// first byte is type, (0xff for done), second is length
				if (tmpBuf[0] == 0xff)
					break;
				
				if (tmpBuf[0] == type && tmpBuf[1] >= 2) {
					
					uint8_t copyLen = gotLen = tmpBuf[1] - 2;
					if (copyLen > maxLen)
						copyLen = maxLen;
					
					eepromRead(2 + addr + ofst, dst, copyLen);
					found = true;
				}
				ofst += tmpBuf[1];
			}
		}
	}
	return found ? gotLen : -1;
}

void boardInitStage2(void)
{
	#ifdef SCREEN_EXPECTS_VCOM
		if (prvReadSetting(0x23, &mScreenVcom, 1) < 0) {
			pr("failed to get VCOM\n");
			mScreenVcom = 0x28;	//a sane value
		}
		pr("VCOM: 0x%02x\n", mScreenVcom);
	#endif
	
	if (prvReadSetting(0x12, &mAdcSlope, 2) < 0) {
		pr("failed to get ADC slope\n");
		mAdcSlope = 2470;	//a sane value
	}
	if (prvReadSetting(0x09, &mAdcIntercept, 2) < 0) {
		pr("failed to get ADC intercept\n");
		mAdcIntercept = 755;	//a sane value
	}
	pr("ADC: %u %u\n", mAdcSlope, mAdcIntercept);
}

__bit boardGetOwnMac(uint8_t __xdata *mac)
{
	uint8_t a, b, c;
	
	if (prvReadSetting(0x2a, mac + 1, 7) < 0 && prvReadSetting(1, mac + 1, 6) < 0)
		return false;

	//reformat MAC how we need it (please do not ask me why it is written so weirdly, ask SDCC. Writing it simpler causes TWO(!!!) spills to DSEG
	
	c = mac[4];
	mac[4] = (uint8_t)(MAC_SECOND_WORD >> 0);
	
	b = mac[5];
	mac[5] = (uint8_t)(MAC_SECOND_WORD >> 8);
	
	a = mac[6];
	mac[6] = (uint8_t)(MAC_SECOND_WORD >> 16);
	mac[7] = (uint8_t)(MAC_SECOND_WORD >> 24);
	
	mac[0] = a;
	mac[1] = b;
	mac[2] = c;
	
	return true;
}


//copied to ram, after update has been verified, interrupts have been disabled, and eepromReadStart() has been called
//does not return (resets using WDT)
//this func wraps the update code and returns its address (in DPTR), len in B
static uint32_t prvUpdateApplierGet(void) __naked
{
	__asm__(
		"	mov   DPTR, #00098$			\n"
		"	mov   A, #00099$			\n"
		"	clr   C						\n"
		"	subb  A, DPL				\n"
		"	mov   B, A					\n"
		"	ret							\n"
		
		///actual updater code
		"00098$:						\n"
	
		"	mov   B, #32				\n"
		//erase all flash
		"	clr   _FADDRH				\n"
		"	clr   _FADDRL				\n"	
		"00001$:						\n"
		"	orl   _FCTL, #0x01			\n"
		"	nop							\n"	//as per datasheet
		"00002$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.7, 00002$			\n"
		"	inc   _FADDRH				\n"
		"	inc   _FADDRH				\n"
		"	djnz  B, 00001$				\n"
		
		//write all 32K
		//due to the 40 usec timeout, we wait each time to avoid it
		"	mov   DPTR, #0			\n"
		
		"00003$:						\n"
		"	mov   _FADDRH, DPH			\n"
		"	mov   _FADDRL, DPL			\n"
		"	inc   DPTR					\n"
		"	mov   _FWT, #0x22			\n"
		
		//get two bytes
	
		"   mov   B, #2					\n"
		
		"00090$:						\n"
		"	mov   _U1DBUF, #0x00		\n"
		
		"00091$:						\n"
		"	mov   A, _U1CSR				\n"
		"	jnb   A.1, 00091$			\n"
		
		"	anl   _U1CSR, #~0x02		\n"
		
		"00092$:						\n"
		"	mov   A, _U1CSR				\n"
		"	jb    A.0, 00092$			\n"
		
		"	push  _U1DBUF				\n"
		"	djnz  B, 00090$				\n"
		
		//write two bytes
		"	orl   _FCTL, #0x02			\n"
		
		//wait for fwbusy to go low
		"00012$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.6, 00012$			\n"
		
		"	pop   A						\n"
		"	pop   _FWDATA				\n"
		"	mov   _FWDATA, A			\n"
		
		//wait for swbusy to be low
		"00004$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.6, 00004$			\n"
		
		"	anl   _FCTL, #~0x02			\n"
		
		//wait for busy to be low
		"00005$:						\n"
		"	mov   A, _FCTL				\n"
		"	jb    A.7, 00005$			\n"
		
		//loop for next two bytes
		"	mov   A, DPH				\n"
		"	cjne  A, #0x40, 00003$		\n"
		
		//done
	
		//WDT reset
		"	mov   _WDCTL, #0x0b			\n"
		"00007$:						\n"
		"	sjmp  00007$				\n"
		
		"00099$:						\n"
	);
}

void selfUpdate(void)
{
	uint32_t updaterInfo = prvUpdateApplierGet();
	
	xMemCopyShort(mScreenRow, (void __xdata*)(uint16_t)updaterInfo, updaterInfo >> 16);
			
	DMAARM = 0xff;	//all DMA channels off
	IEN0 = 0;	//ints off
	
	MEMCTR = 3;	//cache and prefetch off
	
	__asm__(
		"	mov dptr, #_mScreenRow		\n"
		"	clr a						\n"
		"	jmp @a+dptr					\n"
	);
}


void clockingAndIntsInit(void)
{
	uint8_t i, j;
	
	IEN0 = 0;
	IEN1 = 0;
	IEN2 = 0;
	MEMCTR = 0;	//enable prefetch
	
	SLEEP = 0;					//SLEEP.MODE = 0 to use HFXO
	while (!(SLEEP & 0x20));	//wait for HFRC to stabilize
	CLKCON = 0x79;				//high speed RC osc, timer clock is 203.125KHz, 13MHz system clock
	while (!(SLEEP & 0x40));	//wait for HFXO to stabilize
	
	//we need to delay more (chip erratum)
	for (i = 0; i != 128; i++) for (j = 0; j != 128; j++) __asm__ ("nop");
	
	CLKCON = 0x39;				//switch to HFXO
	while (CLKCON & 0x40);		//wait for the switch
	CLKCON = 0x38;				//go to 26MHz system and timer speed,  timer clock is Fosc / 128 = 203.125KHz
	SLEEP = 4;					//power down the unused (HFRC oscillator)
}

uint8_t rndGen8(void)
{
	ADCCON1 |= 4;
	while (ADCCON1 & 0x0c);
	return RNDH ^ RNDL;
}

uint32_t rndGen32(void) __naked
{
	__asm__ (
		//there simply is no way to get SDCC to generate this anywhere near as cleanly
		"	lcall  _rndGen8		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen8		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen8		\n"
		"	push   DPL			\n"
		"	lcall  _rndGen8		\n"
		"	pop    DPH			\n"
		"	pop    B			\n"
		"	pop    A			\n"
		"	ret					\n" 
	);
}

void rndSeed(uint8_t seedA, uint8_t seedB)
{
	RNDL = seedA;
	RNDL = seedB;
}
