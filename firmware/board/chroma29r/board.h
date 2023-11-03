#ifndef _BOARD_H_
#define _BOARD_H_


#include "u1.h"

//colors for ui messages
#define UI_MSG_MAGNIFY1			1
#define UI_MSG_MAGNIFY2			1
#define UI_MSG_MAGNIFY3			1
#define UI_MSG_BACK_COLOR		3
#define UI_MSG_FORE_COLOR_1		0
#define UI_MSG_FORE_COLOR_2		1
#define UI_MSG_FORE_COLOR_3		2
#define UI_BARCODE_VERTICAL

//eeprom spi
#define eepromByte				u1byte
#define eepromPrvSelect()		do { __asm__("nop"); P2_0 = 0; __asm__("nop"); } while(0)
#define eepromPrvDeselect()		do { __asm__("nop"); P2_0 = 1; __asm__("nop"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()				u1setUartMode()
#define dbgUartOff()			u1setEepromMode()
#define dbgUartByte				u1byte

//eeprom map
#define EEPROM_SETTINGS_AREA_START		(0x03000UL)
#define EEPROM_SETTINGS_AREA_LEN		(0x03000UL)
//some free space here
#define EEPROM_UPDATA_AREA_START		(0x06000UL)
#define EEPROM_UPDATE_AREA_LEN			(0x08000UL)
#define EEPROM_IMG_START				(0x0e000UL)
#define EEPROM_IMG_EACH					(0x03000UL)
//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
#define EEPROM_PROGRESS_BYTES			(48)

//radio cfg
#define RADIO_FIRST_CHANNEL				(100)		//sub-GHz channels start at 100
#define RADIO_NUM_CHANNELS				(25)

//hw types
#define HW_TYPE_NORMAL					HW_TYPE_29_INCH_DISPDATA
#define HW_TYPE_CYCLING					HW_TYPE_29_INCH_DISPDATA_FRAME_MODE


#include "../boardCommon.h"


#endif
