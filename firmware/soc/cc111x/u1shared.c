#include "cpu.h"
#include "u1.h"



void u1init(void)
{
   //UART pins
   P1DIR = (P1DIR &~ (1 << 7)) | (1 << 6);      //P1.6 is out, P1.7 is in
   P2SEL |= 1 << 6;
   P1 |= 1 << 6;                       //when it is not uart mode, idle high
   U1UCR = 0b00000010;  //no parity, 8 bits per char, normal polarity
   
   //EEPROM pins
   P0DIR = (P0DIR & (uint8_t)~(1 << 5)) | (1 << 3) | (1 << 4);
   
   P2DIR = (P2DIR & (uint8_t)~(3 << 6)) | (1 << 6);   //usart1 beats usart0 on port 0, p2.0 is output
   
   P0 &= (uint8_t)~(1 << 3);  //clock idles low
   
   u1setEepromMode();
}

uint8_t u1byte(uint8_t v)
{
   U1DBUF = v;
   while (!(U1CSR & 0x02));
   U1CSR &= (uint8_t)~0x02;
   while (U1CSR & 0x01);
   return U1DBUF;
}


/*
   Baud_E   Baud_M   Baudrate          1,000,000 baud Error
   15       59       999755.859375     -0.0245 %
   15       60       1002929.687500    0.29296875 %
*/

//the order of ops in these two functions are very important to avoid glitches on UART.TX
// #define BAUD_115200

void u1setUartMode(void)
{
   P0SEL &= (uint8_t)~((1 << 4) | (1 << 3) | (1 << 5));
      
#ifdef BAUD_115200
   U1BAUD = 34;      //BAUD_M = 34 (115200)
   U1GCR = 0b00001100;  //BAUD_E = 12, lsb first
#else
// Default 1meg baud
   U1BAUD = 60;      //BAUD_M = 60
   U1GCR = 15;       //BAUD_E = 15, lsb first
#endif                        //
   U1CSR = 0b11000000;  //UART mode, RX on
   
   P1SEL |= (uint8_t)((1 << 6) | (1 << 7));
   
   PERCFG |= (uint8_t)(1 << 1);
}

void u1setEepromMode(void)
{
   PERCFG &= (uint8_t)~(1 << 1);
   P1SEL &= (uint8_t)~((1 << 6) | (1 << 7));
   
   U1BAUD = 0;       //F/8 is max for spi - 3.25 MHz
   U1GCR = 0b00110001;  //BAUD_E = 0x11, msb first
   U1CSR = 0b01000000;  //SPI master mode, RX on
   
   P0SEL |= (uint8_t)((1 << 4) | (1 << 3) | (1 << 5));
}

