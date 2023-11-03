#ifndef _SCREEN_H_
#define _SCREEN_H_

#include <stdbool.h>
#include <stdint.h>

//i hate globals, but for 8051 this makes life a lot easier, sorry :(
extern uint8_t __xdata mScreenVcom;
extern int8_t __xdata mCurTemperature;


#define SCREEN_WIDTH				128
#define SCREEN_HEIGHT				296

#define SCREEN_NUM_GREYS			4
#define SCREEN_FIRST_GREY_IDX		0
#define SCREEN_EXTRA_COLOR_INDEX	4		//set to negative if nonexistent
#define SCREEN_TX_BPP				4		//in transit

#define SCREEN_WIDTH_MM				29
#define SCREEN_HEIGHT_MM			67

#define SCREEN_BYTE_FILL			0x33	//white

#define SCREEN_PARTIAL_KEEP			0x00	//full byte filled with value as per SCREEN_TX_BPP
#define SCREEN_PARTIAL_W2B			0x33	//full byte filled with value as per SCREEN_TX_BPP
#define SCREEN_PARTIAL_B2W			0x44	//full byte filled with value as per SCREEN_TX_BPP

#define SCREEN_TYPE					TagScreenEink_BWR_5colors

#define SCREEN_DATA_PASSES			8

void screenShutdown(void);

void screenTest(void);

__bit screenTxStart(__bit forPartial);

void screenEndPass(void);	//at end of each pass

#pragma callee_saves screenByteTx
void screenByteTx(uint8_t byte);
void screenTxEnd(void);



extern uint8_t __xdata mScreenRow[];	//320 bytes used as temp by many on cc where memory is tight

#endif
