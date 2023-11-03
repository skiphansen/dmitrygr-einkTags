#include "cpu.h"
#include "u1.h"



void u1init(void)
{
	//UART config
	U1BAUD = 34;		//BAUD_M = 34 (115200)
	U1GCR = 0b00001100;	//BAUD_E = 12, lsb first
	U1CSR = 0b11000000;	//UART mode, RX on
	U1UCR = 0b00000010;	//no parity, 8 bits per char, normal polarity
	
	//UART pins
	P1DIR = (1 << 6);		//P1.6 is out
	P1SEL |= (1 << 6);
	P2SEL |= 1 << 6;
	PERCFG |= (uint8_t)(1 << 1);
}

uint8_t u1byte(uint8_t v)
{
	U1DBUF = v;
	while (!(U1CSR & 0x02));
	U1CSR &= (uint8_t)~0x02;
	return 0;
}
