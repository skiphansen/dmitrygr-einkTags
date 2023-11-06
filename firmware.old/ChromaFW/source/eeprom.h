#ifndef _EEPROM_H_
#define _EEPROM_H_

#include <stdbool.h>
#include <stdint.h>

#define EEPROM_WRITE_PAGE_SZ		256		//max write size & alignment
#define EEPROM_ERZ_SECTOR_SZ		4096	//erase size and alignment

//device has 256 sectors, so eepromErase() cannot erase thw whole device...i can live with that

__bit eepromInit(void);
void eepromOtpModeEnter(void);
void eepromOtpModeExit(void);
void eepromRead(uint32_t addr, void __xdata *dst, uint16_t len) __reentrant;
bool eepromWrite(uint32_t addr, const void __xdata *src, uint16_t len) __reentrant;
bool eepromErase(uint32_t addr, uint16_t numSectors) __reentrant;

void eepromDeepPowerDown(void);

#pragma callee_saves eepromGetSize
uint32_t eepromGetSize(void);

//this is for firmware update use
void eepromReadStart(uint32_t addr)  __reentrant;

#endif
