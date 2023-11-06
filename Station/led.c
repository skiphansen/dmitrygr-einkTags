#include "nrf52840_bitfields.h"
#include "nrf52840.h"
#include "led.h"



void ledInit(void)
{
	NRF_P0->DIRSET = (1 << 6) | (1 << 8) | (1 << 12);
	NRF_P1->DIRSET = (1 << 8);
	NRF_P0->OUTSET = (1 << 6) | (1 << 8) | (1 << 12);
	NRF_P1->OUTSET = (1 << 8);
	
	NRF_PWM0->PSEL.OUT[0] =  8 | 0x00;	//R
	NRF_PWM0->PSEL.OUT[1] =  9 | 0x20;	//G
	NRF_PWM0->PSEL.OUT[2] = 12 | 0x00;	//B
	NRF_PWM0->PSEL.OUT[3] =  6 | 0x00;	//LED1
	NRF_PWM0->ENABLE = 1;
	NRF_PWM0->MODE = 0;
	NRF_PWM0->PRESCALER = 0;
	NRF_PWM0->COUNTERTOP = 32767;
	NRF_PWM0->LOOP = 0;
	NRF_PWM0->DECODER = 0x102;	//task to reload, one halfword for each led
	NRF_PWM0->SEQ[0].REFRESH = 0;
	ledSet(0,0,0,0);
}

void ledSet(int16_t r, int16_t g, int16_t b, int16_t led2)	//negative not not modify. range 0..255
{
	static volatile uint16_t values[4] = {};
	
	if (r >= 0)
		values[0] = r * r / 2;
	if (g >= 0)
		values[1] = g * g / 2;
	if (b >= 0)
		values[2] = b * b / 2;
	if (led2 >= 0)
		values[3] = led2 * led2 / 2;
	
	NRF_PWM0->SEQ[0].CNT = 4;
	NRF_PWM0->SEQ[0].PTR = (uintptr_t)values;

	//fuck this next step shit that does not work, we'll just disbale and enable!
	if (NRF_PWM0->EVENTS_SEQSTARTED[0]) {
		NRF_PWM0->EVENTS_STOPPED = 0;
		NRF_PWM0->TASKS_STOP = 1;
		while(!NRF_PWM0->EVENTS_STOPPED);
		NRF_PWM0->EVENTS_SEQSTARTED[0] = 0;
	}
	
	NRF_PWM0->TASKS_SEQSTART[0] = 1;
}


