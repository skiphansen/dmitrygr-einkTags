#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stdbool.h>
#include <stdint.h>

//i hate globals, but for 8051 this makes life a lot easier, sorry :(
extern uint8_t __xdata mScreenVcom;
extern int8_t __xdata mCurTemperature;


#define SCREEN_EXPECTS_VCOM

#define SCREEN_WIDTH				640
#define SCREEN_HEIGHT				384

#define SCREEN_NUM_GREYS			7
#define SCREEN_FIRST_GREY_IDX		0
#define SCREEN_EXTRA_COLOR_INDEX	7		//set to negative if nonexistent
#define SCREEN_TX_BPP				4		//in transit

#define SCREEN_WIDTH_MM				163
#define SCREEN_HEIGHT_MM			98

#define SCREEN_BYTE_FILL			0x66	//white for normal mode

#define SCREEN_PARTIAL_KEEP			0x77	//full byte filled with value as per SCREEN_TX_BPP
#define SCREEN_PARTIAL_W2B			0x00	//full byte filled with value as per SCREEN_TX_BPP
#define SCREEN_PARTIAL_B2W			0x11	//full byte filled with value as per SCREEN_TX_BPP

#define SCREEN_TYPE					TagScreenEink_BWY_3bpp


#define SCREEN_DATA_PASSES			1
#define screenEndPass()

void screenShutdown(void);

void screenTest(void);

__bit screenTxStart(__bit forPartial);

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte);
void screenTxEnd(void);

void P1INT_ISR(void) __interrupt (15);


__xdata __at (0xfda2) uint8_t mScreenRow[320];	//350 bytes here, we use 320 of them

#endif
