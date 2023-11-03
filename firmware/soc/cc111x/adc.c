#include <stdbool.h>
#include "asmUtil.h"
#include "printf.h"
#include "cpu.h"
#include "adc.h"

uint16_t __xdata mAdcSlope;		//token 0x12
uint16_t __xdata mAdcIntercept;	//token 0x09

//adc is not really 12 bits in single ended mode. max is 0x7ff

static uint16_t adcPrvSample(uint8_t input)	//virtual sources only. returns adcval << 4
{
	uint16_t ret;
	
	ADCH = 0;
	ADCCON2 = 0x30 | input;
	ADCCON1 = 0x73;
	while (ADCCON1 & 0x40);
	while (!(ADCCON1 & 0x80));
	
	ret = ADCL;
	ret |= (((uint16_t)ADCH << 8));
	
	ADCCON2 = 0;
	
	return ret;
}

uint16_t adcSampleBattery(void)
{
	return mathPrvMul16x8(mathPrvDiv16x8(adcPrvSample(15), 35), 4);	//within 3%
}

int8_t adcSampleTemperature(void)
{
	__bit neg = false;
	uint32_t val;
	uint8_t ret;
	
	val = mathPrvMul16x16(adcPrvSample(14) >> 4, 1250) - mathPrvMul16x16(mAdcIntercept, 0x7ff);
	//*= 1000
	val = mathPrvMul32x8(val, 125);
	val <<= 3;
	
	if (val & 0x80000000) {
		neg = true;
		val = -val;
	}
	
	ret = mathPrvDiv32x16(mathPrvDiv32x16(val, mAdcSlope) + 0x7ff / 2, 0x7ff);
	
	if (neg)
		ret = -ret;
	
	return ret;
	
	//to get calibrated temp in units of 0.1 degreesC
	// (sext(AdcVal) * 1250 - uext(tokenGet(0x09)) * 0x7ff) * 1000 / tokenGet(0x12) * 10 / 0x7ff
	//if token 0x12 is missing (as it is on some devices, use 2470)
	//if token 0x09 is missing (as it is on some devices, use 755)
}
