#ifndef _RADIO_H_
#define _RADIO_H_

#include <stdbool.h>
#include <stdint.h>


void DMA_ISR(void) __interrupt (8);
#define RADIO_PAD_LEN_BY		0
	
#include "../radioCommon.h"



#endif




