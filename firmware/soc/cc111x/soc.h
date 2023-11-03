#ifndef _SOCi_H_
#define _SOCi_H_

#define PDATA __pdata
#include "cc111x.h"

#include <stdint.h>

void WOR_ISR(void) __interrupt (5);

#pragma callee_saves clockingAndIntsInit
void clockingAndIntsInit(void);

#pragma callee_saves rndGen8
uint8_t rndGen8(void);

#pragma callee_saves rndGen32
uint32_t rndGen32(void);

#pragma callee_saves rndSeed
void rndSeed(uint8_t seedA, uint8_t seedB);

#pragma callee_saves selfUpdate
void selfUpdate(void);





#endif
