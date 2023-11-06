#include "nrf52840_bitfields.h"
#include "nrf52840.h"
#include <string.h>
#include "aes.h"


void aesEnc(const void* keyP, const void *dataP, void* outP)
{
	unsigned char nfo[48];
	
	memcpy(nfo, keyP, 16);
	memcpy(nfo + 16, dataP, 16);
	
	NRF_ECB->ECBDATAPTR = (uintptr_t)nfo;
	NRF_ECB->EVENTS_ENDECB = 0;
	asm volatile("":::"memory");
	NRF_ECB->TASKS_STARTECB = 1;
	while (!NRF_ECB->EVENTS_ENDECB);
	asm volatile("":::"memory");
	memcpy(outP, nfo + 32, 16);
}


