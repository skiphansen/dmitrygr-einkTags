#ifndef _EEPROM_MAP_H_
#define _EEPROM_MAP_H_

#include <stdint.h>
#include "eeprom.h"

#if defined(CHROMA74)

	#define EEPROM_SETTINGS_AREA_START		(0x08000UL)
	#define EEPROM_SETTINGS_AREA_LEN		(0x04000UL)
	
	//some free space here
	
	#define EEPROM_UPDATA_AREA_START		(0x10000UL)
	#define EEPROM_UPDATE_AREA_LEN			(0x09000UL)
	
	#define EEPROM_IMG_START				(0x19000UL)
	#define EEPROM_IMG_EACH					(0x17000UL)
	//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
	
	
	#define EEPROM_PROGRESS_BYTES			(192)
	
#elif defined(CHROMA29)

	#define EEPROM_SETTINGS_AREA_START		(0x03000UL)
	#define EEPROM_SETTINGS_AREA_LEN		(0x03000UL)
	
	//some free space here
	
	#define EEPROM_UPDATA_AREA_START		(0x06000UL)
	#define EEPROM_UPDATE_AREA_LEN			(0x08000UL)
	
	#define EEPROM_IMG_START				(0x0e000UL)
	#define EEPROM_IMG_EACH					(0x03000UL)
	//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
	
	
	#define EEPROM_PROGRESS_BYTES			(48)
	
#elif defined(EPOP900)

	#define EEPROM_SETTINGS_AREA_START		(0x08000UL)
	#define EEPROM_SETTINGS_AREA_LEN		(0x04000UL)
	
	//some free space here
	
	#define EEPROM_UPDATA_AREA_START		(0x10000UL)
	#define EEPROM_UPDATE_AREA_LEN			(0x09000UL)
	
	#define EEPROM_IMG_START				(0x19000UL)
	#define EEPROM_IMG_EACH					(0x06000UL)
	//till end of eeprom really. do not put anything after - it will be erased at pairing time!!!
	
	#define EEPROM_PROGRESS_BYTES			(48)
	
#elif defined(EPOP50)
	
	//todo
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
	
#else
#error "device type not known"
#endif

#define EEPROM_IMG_INPROGRESS			(0x7fffffffUL)
#define EEPROM_IMG_VALID				(0x494d4721UL)

#define EEPROM_PIECE_SZ					(88)
struct EepromImageHeader {				//each image space is 0x17000 bytes, we have space for ten of them
	uint64_t version;
	uint32_t validMarker;
	uint32_t size;
	uint32_t rfu[8];									//zero-filled for now
	uint8_t piecesMissing[EEPROM_PROGRESS_BYTES];		//each bit represents a EEPROM_PIECE_SZ-byte piece
	
	//image data here
	//we pre-erase so progress can be calculated by finding the first non-0xff byte
};

#if (EEPROM_SETTINGS_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "settings area must be an integer number of eeprom blocks"
#endif

#if (EEPROM_SETTINGS_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "settings must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_EACH % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "each image must be an integer number of eeprom blocks"
#endif

#if (EEPROM_IMG_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "images must begin at an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATE_AREA_LEN % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "update must be an integer number of eeprom blocks"
#endif

#if (EEPROM_UPDATA_AREA_START % EEPROM_ERZ_SECTOR_SZ) != 0
	#error "images must begin at an integer number of eeprom blocks"
#endif





#endif

