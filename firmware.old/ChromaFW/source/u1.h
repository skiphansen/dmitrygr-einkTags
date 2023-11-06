#ifndef _U1_H_
#define _U1_H_

//uart1 is shared between eeprom and console UART, hence one include file
// defaults to eeprom mode. printf() will also reset to that!

#include <stdint.h>

void u1init(void);


#pragma callee_saves u1byte
#pragma callee_saves u1setUartMode
#pragma callee_saves u1setEepromMode


uint8_t u1byte(uint8_t v);
void u1setUartMode(void);
void u1setEepromMode(void);



#endif
