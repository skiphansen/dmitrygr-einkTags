#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <stdint.h>

//pages are 4K in size
//an update can be stored in any 2 image slots

#define EEPROM_PAGE_SIZE			(0x01000)

#define EEPROM_OS_START				(0x00000)
#define EEPROM_OS_LEN				(0x0F000)	//0xE820 of image, rounded up to 4K

#define EEPROM_CHARS_IMGS_START		(0x0F000)
#define EEPROM_CHARS_IMGS_LEN		(0x01000)

#define EEPROM_IMG_START			(0x10000)
#define EEPROM_IMG_EACH				(0x08000)	//300x400x2bpp + header + rounding or update chunk
#define EEPROM_IMG_LEN				(0x58000)

#define EEPROM_SETTINGS_AREA_START	(0x68000)
#define EEPROM_SETTINGS_AREA_LEN	(0x04000)

#define EEPROM_MAC_INFO_START		(0x6c000)	//not same as stock
#define EEPROM_MAC_INFO_LEN			(0x01000)

#define EEPROM_UPDATE_START			(0x6f000)	//same header as images
#define EEPROM_UPDATE_LEN			(0x11000)

#define EEPROM_IMG_INPROGRESS		(0x7fffffff)
#define EEPROM_IMG_VALID			(0x494d4721)


#define EEPROM_PIECE_SZ				(88)
struct EepromImageHeader {
	uint64_t version;
	uint32_t validMarker;
	uint32_t size;
	uint32_t rfu[8];				//zero-filled for now
	uint8_t piecesMissing[128];		//each bit represents a 64-byte piece
	
	//image data here
	//os update here possibly (EEPROM_OS_UPDATE_SZ_PER_IMG bytes each piece)
	//we pre-erase so progress can be calculated by finding the first non-0xff byte
};


#endif
