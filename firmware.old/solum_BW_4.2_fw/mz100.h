#ifndef _MZ100_H_
#define _MZ100_H_


typedef enum {
	Reset_IRQn                = -15,
	NonMaskableInt_IRQn       = -14,
	HardFault_IRQn            = -13,
	MemoryManagement_IRQn     = -12,
	BusFault_IRQn             = -11,
	UsageFault_IRQn           = -10,
	SVCall_IRQn               =  -5,
	DebugMonitor_IRQn         =  -4,
	PendSV_IRQn               =  -2,
	SysTick_IRQn              =  -1,

	Rtc_IRQn = 0,
	BrnDet_IRQn = 6,
	LpComp_IRQn = 7,
	ADC_IRQn = 8,
	DAC_IRQn = 9,
	Comp_IRQn = 10,
	CRC_IRQn = 11,
	AES_IRQn = 12,
	I2C_1_IRQn = 13,
	I2C_2_IRQn = 14,
	DMA_IRQn = 15,
	GPIO_IRQn = 16,
	SSP_1_IRQn = 17,
	SSP_2_IRQn = 18,
	QSPI_IRQn = 19,
	GPT_1_IRQn = 20,
	GPT_2_IRQn = 21,
	UART_1_IRQn = 22,
	UART_2_IRQn = 23,
	PHY_INT_1_IRQn = 24,
	PHY_INT_2_IRQn = 25,
	PHY_INT_3_IRQn = 26,
	PHY_INT_4_IRQn = 27,
	ThreeDG_IRQn = 28,
	KeyScan_IRQn = 29,
	Infrared_IRQn = 30,
	QuadEnc_IRQn = 31,
	EXT_GPIO_1_IRQn = 31,
	EXT_GPIO_2_IRQn = 32,
	EXT_GPIO_3_IRQn = 33,
	EXT_GPIO_4_IRQn = 34,
	EXT_GPIO_5_IRQn = 35,
	EXT_GPIO_6_IRQn = 36,
	EXT_GPIO_7_IRQn = 37,
	EXT_GPIO_8_IRQn = 38,
	EXT_GPIO_9_IRQn = 39,
	EXT_GPIO_10_IRQn = 40,
	EXT_GPIO_11_IRQn = 41,
	EXT_GPIO_12_IRQn = 42,
	EXT_GPIO_13_IRQn = 43,
	EXT_GPIO_14_IRQn = 44,
	EXT_GPIO_15_IRQn = 46,
	EXT_GPIO_16_IRQn = 47,
	EXT_GPIO_17_IRQn = 48,
	EXT_GPIO_18_IRQn = 49,
	EXT_GPIO_19_IRQn = 50,
	EXT_GPIO_20_IRQn = 51,
	EXT_GPIO_21_IRQn = 51,
	EXT_GPIO_22_IRQn = 52,
	EXT_GPIO_23_IRQn = 53,
	EXT_GPIO_24_IRQn = 54,
	EXT_GPIO_25_IRQn = 56,
	EXT_GPIO_26_IRQn = 57,
	EXT_GPIO_27_IRQn = 58,
	EXT_GPIO_28_IRQn = 59,
	EXT_GPIO_29_IRQn = 60,
	EXT_GPIO_30_IRQn = 61,
	EXT_GPIO_31_IRQn = 62,
} IRQn_Type;

#define __NVIC_PRIO_BITS	4

#include "core_cm3.h"


struct MzAes {
	volatile uint32_t CTRL1;
	volatile uint32_t CTRL2;
	volatile uint32_t STATUS;
	volatile uint32_t ASTR_LEN;
	volatile uint32_t MSTR_LEN;
	volatile uint32_t STR_IN;
	volatile uint32_t IV[4];
	volatile uint32_t KEY[8];
	volatile uint32_t STR_OUT;
	volatile uint32_t OV[4];
	volatile uint32_t ISR;
	volatile uint32_t IMR;
	volatile uint32_t IRSR;
	volatile uint32_t ICR;
	volatile uint32_t REVID;
};

struct MzGptChannel {
	volatile uint32_t CNTL;
	uint32_t rfu0[1];
	volatile uint32_t ACT;
	uint32_t rfu1[1];
	volatile uint32_t CUP_VAL;
	uint32_t rfu2[3];
	volatile uint32_t CDN_VAL;
	uint32_t rfu3[7];
};

struct MzGpt {
	volatile uint32_t CNT_EN;
	uint32_t rfu0[7];
	volatile uint32_t INT_RAW;
	volatile uint32_t INT;
	volatile uint32_t INT_MSK;
	uint32_t rfu1[5];
	volatile uint32_t CNT_CNTL;
	uint32_t rfu2[3];
	volatile uint32_t CNT_VAL;
	uint32_t rfu3[3];
	volatile uint32_t CNT_UPP_VAL;
	uint32_t rfu4[7];
	volatile uint32_t CLK_CNTL;
	uint32_t rfu5[1];
	volatile uint32_t IO_CNTL;
	uint32_t rfu6[5];
	volatile uint32_t DMA_CNTL_EN;
	volatile uint32_t DMA_CNTL_CH;
	uint32_t rfu7[18];
	volatile uint32_t USER_REQ;
	uint32_t rfu8[67];
	struct MzGptChannel ch[5];
};

struct MzAdc {
	volatile uint32_t PWR;
	volatile uint32_t CLKRST;
	volatile uint32_t CMD;
	uint32_t rfu1[1];
	volatile uint32_t ANA;
	volatile uint32_t DMAR;
	uint32_t rfu2[1];
	volatile uint32_t STATUS;
	volatile uint32_t ISR;
	volatile uint32_t IMR;
	volatile uint32_t IRSR;
	volatile uint32_t ICR;
	volatile uint32_t RESULT;
	uint32_t rfu3[1];
	volatile uint32_t OFF_CAL;
	volatile uint32_t GAIN_CAL;
	uint32_t rfu4[1];
	volatile uint32_t AUDIO;
	volatile uint32_t VOICE_DET;
	volatile uint32_t RESULT_BUF;
};

struct MzPmu {
	uint32_t rfu1[1];
	
	volatile uint32_t PWR_STA;				//0x04: clock and power status buts
	volatile uint32_t CLK_SRC;				//0x08: clock dividers for various parts of the cpu
	volatile uint32_t FREQ;					//0x0c: actual req???
	
	uint32_t rfu2[4];
	
	volatile uint32_t POWERDOWN;			//0x20: set bits to disable units
};

struct MzWdt {
	volatile uint32_t CR;
	volatile uint32_t TORR;
	volatile uint32_t CCVR;
	volatile uint32_t CRR;
};

#define MZ_PMU_CLK_SRC_GPT2_SRC_MASK		0x00000030
#define MZ_PMU_CLK_SRC_GPT2_SRC_32M_XTAL	0x00000000
#define MZ_PMU_CLK_SRC_GPT2_SRC_32M_RC		0x00000010
#define MZ_PMU_CLK_SRC_GPT2_SRC_32K_XTAL	0x00000020
#define MZ_PMU_CLK_SRC_GPT2_SRC_32K_RC		0x00000030
#define MZ_PMU_CLK_SRC_GPT1_SRC_MASK		0x0000000c
#define MZ_PMU_CLK_SRC_GPT1_SRC_32M_XTAL	0x00000000
#define MZ_PMU_CLK_SRC_GPT1_SRC_32M_RC		0x00000004
#define MZ_PMU_CLK_SRC_GPT1_SRC_32K_XTAL	0x00000008
#define MZ_PMU_CLK_SRC_GPT1_SRC_32K_RC		0x0000000c
//bit 0x00000002 is likely clock source selection for RTC unit
#define MZ_PMU_CLK_SRC_CPU_DIV2				0x00000001 //if set cpu is 32mhz, else 64. afects uart source clock too

#define MZ_PMU_POWERDOWN_3DGLASS			0x10000000
#define MZ_PMU_POWERDOWN_IR					0x08000000
#define MZ_PMU_POWERDOWN_KEYSCAN			0x02000000
#define MZ_PMU_POWERDOWN_DMA				0x00200000
#define MZ_PMU_POWERDOWN_AES_AND_CRC		0x00080000
#define MZ_PMU_POWERDOWN_I2C1				0x00020000
#define MZ_PMU_POWERDOWN_I2C2				0x00010000
#define MZ_PMU_POWERDOWN_UART1				0x00008000
#define MZ_PMU_POWERDOWN_UART2				0x00004000
#define MZ_PMU_POWERDOWN_WDT				0x00002000
#define MZ_PMU_POWERDOWN_BR_RC32M_XTAL32M	0x00001000
#define MZ_PMU_POWERDOWN_RTC				0x00000800
#define MZ_PMU_POWERDOWN_PINMUX				0x00000400
#define MZ_PMU_POWERDOWN_GPT1				0x00000200
#define MZ_PMU_POWERDOWN_GPT2				0x00000100
#define MZ_PMU_POWERDOWN_GPIO				0x00000080
#define MZ_PMU_POWERDOWN_QSPI				0x00000040
#define MZ_PMU_POWERDOWN_SSP1				0x00000020
#define MZ_PMU_POWERDOWN_SSP2				0x00000010
#define MZ_PMU_POWERDOWN_ADC_DAC_ACOMP		0x00000008
#define MZ_PMU_POWERDOWN_SYSCTRL			0x00000004
#define MZ_PMU_POWERDOWN_CLKOUT				0x00000001



#define AES		((struct MzAes*)0x45000000)
#define WDT		((struct MzWdt*)0x48030000)
#define GPT1	((struct MzGpt*)0x4A010000)
#define GPT2	((struct MzGpt*)0x4A020000)
#define ADC		((struct MzAdc*)0x4A030000)
#define PMU		((struct MzPmu*)0x4A080000)

#endif
