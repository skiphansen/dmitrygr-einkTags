#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>
#include <stdint.h>



uint32_t rnd32(void);
uint32_t measureTemp(void);
uint32_t measureBattery(void);	//return mV

void qspiEraseRange(uint32_t addr, uint32_t len);		//will over-erase to  round up to erase block boundary

void setPowerState(bool fast);	//fast mode required for going to sleep...do not ask

void radioShutdown(void);		//experimentally written. suggest reset after use to bring radio back :)

#endif
