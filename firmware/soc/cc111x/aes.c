#include "aes.h"
#include "cpu.h"



void aesSetKey(const uint8_t __xdata *key)
{
	uint8_t i;
	
	//upload key
	ENCCS = 0x05;
	for (i = 0; i < AES_KEY_SIZE; i++)
		ENCDI = *key++;
	while (!(ENCCS & 0x08));
}

void aesEnc(uint8_t __xdata *data)
{
	const uint8_t __xdata *src = data;
	uint8_t i;
	
	ENCCS = 0x41;
	
	for (i = 0; i < AES_BLOCK_SIZE; i++)
		ENCDI = *src++;
	
	__asm__(
		"	push ar0				\n"
		"	mov  r0, #22			\n"
		"000001$:					\n"
		"	djnz r0, 000001$		\n"
		"	pop  ar0				\n"
	);
	
	for (i = 0; i < AES_BLOCK_SIZE; i++)
		*data++ = ENCDO;
	
	while (!(ENCCS & 0x08));
}