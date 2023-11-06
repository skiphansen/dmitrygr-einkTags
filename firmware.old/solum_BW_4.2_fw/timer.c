#include "timer.h"
#include "mz100.h"
#include "fw.h"


static volatile uint32_t mTicksHi = 0;

static void timerPrvIrqh(void)
{
	GPT1->INT_RAW = 1 << 16;
	(void)GPT1->INT_RAW;	//readback - errata for irq double-triggering
	mTicksHi++;
}

void timerInit(void)
{
	//set GPT1 timer source to 32MHz
	PMU->CLK_SRC = (PMU->CLK_SRC &~MZ_PMU_CLK_SRC_GPT1_SRC_MASK) | MZ_PMU_CLK_SRC_GPT1_SRC_32M_XTAL;
	PMU->CLK_SRC |= (MZ_PMU_CLK_SRC_GPT1_SRC_32M_XTAL << 2);
	
	
	GPT1->CNT_EN = 2;	//stop it
	VECTORS[GPT_1_IRQn + 16] = &timerPrvIrqh;
	NVIC_EnableIRQ(20);
	NVIC_ClearPendingIRQ(20);
	
	GPT1->INT_MSK = ~(1 << 16);
	GPT1->CNT_CNTL = 0x100;		//count up, slow value updates
	GPT1->CNT_UPP_VAL = 0xffffffff;
	GPT1->CLK_CNTL = 0x000;	//Fclk / 1. Fclk is rc32K - 32KHz-ish. PMU can change it, somehow..haha
	GPT1->DMA_CNTL_EN = 0;
	GPT1->CNT_EN = 5;	//reset and start
}

void timerStop(void)
{
	GPT1->CNT_EN = 0;
}

uint64_t timerGet(void)
{
	uint32_t hi, lo;
	
	do {
		hi = mTicksHi;
		lo = GPT1->CNT_VAL;
	} while (hi != mTicksHi);
	
	return (((uint64_t)hi) << 32) + lo;
}

void timerDelay(uint64_t cycles)
{
	uint64_t t;
	
	t = timerGet();
	while (timerGet() - t < cycles);
}