#ifndef _BOARD_H_
#define _BOARD_H_

#include "u1.h"

//colors for ui messages
#define UI_MSG_MAGNIFY1			1
#define UI_MSG_MAGNIFY2			1
#define UI_MSG_MAGNIFY3			1
#define UI_MSG_BACK_COLOR		1
#define UI_MSG_FORE_COLOR_1		0
#define UI_MSG_FORE_COLOR_2		0
#define UI_MSG_FORE_COLOR_3		0
#define UI_BARCODE_HORIZ

//eeprom spi
#define eepromByte				u1byte
#define eepromPrvSelect()		do { __asm__("nop"); P2_0 = 0; __asm__("nop"); } while(0)
#define eepromPrvDeselect()		do { __asm__("nop"); P2_0 = 1; __asm__("nop"); } while(0)

//debug uart (enable only when needed, on some boards it inhibits eeprom access)
#define dbgUartOn()				u1setUartMode()
#define dbgUartOff()			u1setEepromMode()
#define dbgUartByte				u1byte

//eeprom map
#define EEPROM_SETTINGS_AREA_START		(0x04000UL)
#define EEPROM_SETTINGS_AREA_LEN		(0x02000UL)
//some free space here
//original rom stores tokens at 0x6000 or 0x7000
#define EEPROM_UPDATA_AREA_START		(0x08000UL)
#define EEPROM_UPDATE_AREA_LEN			(0x08000UL)
#define EEPROM_IMG_START				(0x00000UL)
#define EEPROM_IMG_EACH					(0x01000UL)
#define EEPROM_IMG_LEN					(0x04000UL)
#define EEPROM_PROGRESS_BYTES			(48)

//radio cfg
#define RADIO_FIRST_CHANNEL				(100)		//sub-GHz channels start at 100
#define RADIO_NUM_CHANNELS				(25)

//hw types
#define HW_TYPE_NORMAL					HW_TYPE_ZBD_EPOP50



#include "../boardCommon.h"



#endif
