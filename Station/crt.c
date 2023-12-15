#include "nrf52840.h"

#define WEAK __attribute__ ((weak))
#define ALIAS(f) __attribute__((noreturn,weak, alias (#f)))



void IntDefaultHandler(void);
void NMI_Handler(void) ALIAS(IntDefaultHandler);
void HardFault_Handler(void) ALIAS(IntDefaultHandler);
void MemUsage_Handler(void) ALIAS(IntDefaultHandler);
void BusFault_Handler(void) ALIAS(IntDefaultHandler);
void UsageFault_Handler(void) ALIAS(IntDefaultHandler);
void SVC_Handler(void) ALIAS(IntDefaultHandler);
void DebugMon_Handler(void) ALIAS(IntDefaultHandler);
void PendSV_Handler(void) ALIAS(IntDefaultHandler);
void SysTick_Handler(void) ALIAS(IntDefaultHandler);

void POWER_CLOCK_IRQHandler(void) ALIAS(IntDefaultHandler);
void RADIO_IRQHandler(void) ALIAS(IntDefaultHandler);
void UARTE0_UART0_IRQHandler(void) ALIAS(IntDefaultHandler);
void SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler(void) ALIAS(IntDefaultHandler);
void SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler(void) ALIAS(IntDefaultHandler);
void NFCT_IRQHandler(void) ALIAS(IntDefaultHandler);
void GPIOTE_IRQHandler(void) ALIAS(IntDefaultHandler);
void SAADC_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER0_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER1_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER2_IRQHandler(void) ALIAS(IntDefaultHandler);
void RTC0_IRQHandler(void) ALIAS(IntDefaultHandler);
void TEMP_IRQHandler(void) ALIAS(IntDefaultHandler);
void RNG_IRQHandler(void) ALIAS(IntDefaultHandler);
void ECB_IRQHandler(void) ALIAS(IntDefaultHandler);
void CCM_AAR_IRQHandler(void) ALIAS(IntDefaultHandler);
void WDT_IRQHandler(void) ALIAS(IntDefaultHandler);
void RTC1_IRQHandler(void) ALIAS(IntDefaultHandler);
void QDEC_IRQHandler(void) ALIAS(IntDefaultHandler);
void COMP_LPCOMP_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI0_EGU0_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI1_EGU1_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI2_EGU2_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI3_EGU3_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI4_EGU4_IRQHandler(void) ALIAS(IntDefaultHandler);
void SWI5_EGU5_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER3_IRQHandler(void) ALIAS(IntDefaultHandler);
void TIMER4_IRQHandler(void) ALIAS(IntDefaultHandler);
void PWM0_IRQHandler(void) ALIAS(IntDefaultHandler);
void PDM_IRQHandler(void) ALIAS(IntDefaultHandler);
void MWU_IRQHandler(void) ALIAS(IntDefaultHandler);
void PWM1_IRQHandler(void) ALIAS(IntDefaultHandler);
void PWM2_IRQHandler(void) ALIAS(IntDefaultHandler);
void SPIM2_SPIS2_SPI2_IRQHandler(void) ALIAS(IntDefaultHandler);
void RTC2_IRQHandler(void) ALIAS(IntDefaultHandler);
void I2S_IRQHandler(void) ALIAS(IntDefaultHandler);
void FPU_IRQHandler(void) ALIAS(IntDefaultHandler);
void USBD_IRQHandler(void) ALIAS(IntDefaultHandler);
void UARTE1_IRQHandler(void) ALIAS(IntDefaultHandler);
void QSPI_IRQHandler(void) ALIAS(IntDefaultHandler);
void CRYPTOCELL_IRQHandler(void) ALIAS(IntDefaultHandler);
void PWM3_IRQHandler(void) ALIAS(IntDefaultHandler);
void SPIM3_IRQHandler(void) ALIAS(IntDefaultHandler);

//main must exist
extern int main(void);

//stack top (provided by linker)
extern void __stack_top();
extern void __data_data();
extern void __data_start();
extern void __data_end();
extern void __bss_start();
extern void __bss_end();

static void ResetISR(void);



#define INFINITE_LOOP_LOW_POWER		while (1) {				\
							asm("wfi":::"memory");	\
						}


void __attribute__((noreturn)) IntDefaultHandler(void)
{
	INFINITE_LOOP_LOW_POWER
}


//vector table
__attribute__ ((section(".vectors"))) void (*const __VECTORS[]) (void) =
{
	&__stack_top,
	ResetISR,
	NMI_Handler,
	HardFault_Handler,
	
	MemUsage_Handler,
	BusFault_Handler,
	UsageFault_Handler,
	0,
	0,
	0,
	0,
	SVC_Handler,
	DebugMon_Handler,
	0,
	PendSV_Handler,
	SysTick_Handler,
	
	POWER_CLOCK_IRQHandler,
	RADIO_IRQHandler,
	UARTE0_UART0_IRQHandler,
	SPIM0_SPIS0_TWIM0_TWIS0_SPI0_TWI0_IRQHandler,
	SPIM1_SPIS1_TWIM1_TWIS1_SPI1_TWI1_IRQHandler,
	NFCT_IRQHandler,
	GPIOTE_IRQHandler,
	SAADC_IRQHandler,
	TIMER0_IRQHandler,
	TIMER1_IRQHandler,
	TIMER2_IRQHandler,
	RTC0_IRQHandler,
	TEMP_IRQHandler,
	RNG_IRQHandler,
	ECB_IRQHandler,
	CCM_AAR_IRQHandler,
	WDT_IRQHandler,
	RTC1_IRQHandler,
	QDEC_IRQHandler,
	COMP_LPCOMP_IRQHandler,
	SWI0_EGU0_IRQHandler,
	SWI1_EGU1_IRQHandler,
	SWI2_EGU2_IRQHandler,
	SWI3_EGU3_IRQHandler,
	SWI4_EGU4_IRQHandler,
	SWI5_EGU5_IRQHandler,
	TIMER3_IRQHandler,
	TIMER4_IRQHandler,
	PWM0_IRQHandler,
	PDM_IRQHandler,
	0,
	0,
	MWU_IRQHandler,
	PWM1_IRQHandler,
	PWM2_IRQHandler,
	SPIM2_SPIS2_SPI2_IRQHandler,
	RTC2_IRQHandler,
	I2S_IRQHandler,
	FPU_IRQHandler,
	USBD_IRQHandler,
	UARTE1_IRQHandler,
	QSPI_IRQHandler,
	CRYPTOCELL_IRQHandler,
	0,
	0,
	PWM3_IRQHandler,
	0,
	SPIM3_IRQHandler,
};

static void __attribute__((noreturn)) ResetISR(void)
{
	unsigned int *dst, *src, *end;

	//copy data
	dst = (unsigned int*)&__data_start;
	src = (unsigned int*)&__data_data;
	end = (unsigned int*)&__data_end;
	while(dst != end)
		*dst++ = *src++;

	//init bss
	dst = (unsigned int*)&__bss_start;
	end = (unsigned int*)&__bss_end;
	while(dst != end)
		*dst++ = 0;

	SCB->VTOR = (uintptr_t)__VECTORS;

	main();

	INFINITE_LOOP_LOW_POWER
}

