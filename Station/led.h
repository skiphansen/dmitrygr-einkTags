#ifndef _LED_H_
#define _LED_H_

#include <stdint.h>

void ledInit(void);

void ledSet(int16_t r, int16_t g, int16_t b, int16_t led2); //negative not not modify. range 0..255



#endif
