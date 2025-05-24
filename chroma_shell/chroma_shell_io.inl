#ifndef __CHROMA_SHELL_IO_
#include <unistd.h>
#include "logging.h"
#define __OEPL_OELP__
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_byte(a) (*(uint8_t *)a)
#define pgm_read_word(a) (*(uint16_t *)a)
#define pgm_read_dword(a) (*(uint32_t *)a)
#define LOW 0
#define HIGH 1
#ifndef I2C_SLAVE
#define I2C_SLAVE 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#endif

// forward references
void bbepWakeUp(BBEPDISP *pBBEP);
void bbepInitIO(BBEPDISP *pBBEP, uint32_t u32Speed);
void bbepWriteData(BBEPDISP *pBBEP, uint8_t *pData, int iLen);
void bbepCMD2(BBEPDISP *pBBEP, uint8_t cmd1, uint8_t cmd2);
void bbepWriteCmd(BBEPDISP *pBBEP, uint8_t cmd);
long millis(void);
long micros(void);
void bbepSetCS2(BBEPDISP *pBBEP, uint8_t cs);
void digitalWrite(int iPin, int iState);
int digitalRead(int iPin);
void delay(int iTime);


#endif // __CHROMA_SHELL_IO_
