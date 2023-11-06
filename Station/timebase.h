#ifndef _TIMEBASE_H_
#define _TIMEBASE_H_

#include <stdint.h>


#define TIMER_TICKS_PER_SECOND		(64000000ULL)

void timebaseInit(void);
uint64_t timebaseGet(void);


#endif
