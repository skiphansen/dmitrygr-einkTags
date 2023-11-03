#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

#define TIMER_TICKS_PER_SECOND			(26000000 / 128 / 8)

//this is a requirement by SDCC. is this prototype is missing when compiling main(), we get no irq handler
void T1_ISR(void) __interrupt (9);


void timerInit(void);

#pragma callee_saves timerGet
uint32_t timerGet(void);

#pragma callee_saves timerGetLowBits
uint8_t timerGetLowBits(void);	//probaly only useful for random seeds

void timerDelay(uint32_t ticks);



#endif
