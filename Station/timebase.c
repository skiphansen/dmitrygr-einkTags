#include "nrf52840.h"
#include "timebase.h"


static uint32_t mTicks = 0;

void __attribute__((used)) SysTick_Handler(void)
{
	mTicks++;
}

uint64_t timebaseGet(void)
{
	uint32_t hi, lo;
	
	do {
		asm volatile("":::"memory");
		hi = mTicks;
		asm volatile("":::"memory");
		lo = SysTick->VAL;
		asm volatile("":::"memory");
	} while (hi != mTicks);
	
	return (((uint64_t)hi) << 24) + (0x00fffffful - lo);
}

void timebaseInit(void)
{
	SysTick->LOAD = 0x00fffffful;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
	NVIC_SetPriority(SysTick_IRQn, 2);	//very important to keep time properly
}