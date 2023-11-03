#ifndef _BOARD_H_
#define _BOARD_H_


#include "spi.h"
#include "uart.h"

//colors for ui messages
#define UI_MSG_MAGNIFY1			1
#define UI_MSG_MAGNIFY2			1
#define UI_MSG_MAGNIFY3			1
#define UI_MSG_BACK_COLOR		3
#define UI_MSG_FORE_COLOR_1		0
#define UI_MSG_FORE_COLOR_2		1
#define UI_MSG_FORE_COLOR_3		2
#define UI_BARCODE_VERTICAL

#define eepromByte				spiByte
#define eepromPrvSelect()		do { __asm__("nop\nnop\nnop\n"); P1_1 = 0; __asm__("nop\nnop\nnop\n"); } while(0)
#define eepromPrvDeselect()		do { __asm__("nop\nnop\nnop\n"); P1_1 = 1; __asm__("nop\nnop\nnop\n"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()
#define dbgUartOff()
#define dbgUartByte				uartTx

//eeprom map
#define EEPROM_SETTINGS_AREA_START		(0x01000UL)
#define EEPROM_SETTINGS_AREA_LEN		(0x03000UL)
#define EEPROM_UPDATA_AREA_START		(0x04000UL)
#define EEPROM_UPDATE_AREA_LEN			(0x10000UL)
#define EEPROM_IMG_START				(0x14000UL)
#define EEPROM_IMG_EACH					(0x03000UL)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES			(128)

//radio cfg
#define RADIO_FIRST_CHANNEL				(11)		//2.4-GHz channels start at 11
#define RADIO_NUM_CHANNELS				(16)

//hw types
#define HW_TYPE_NORMAL					HW_TYPE_29_INCH_ZBS_025
#define HW_TYPE_CYCLING					HW_TYPE_29_INCH_ZBS_025_FRAME_MODE


#include "../boardCommon.h"



#endif
