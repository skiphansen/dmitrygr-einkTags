/*
   EPD_Driver.cpp
   
   --COPYRIGHT--
  * Brief The drivers and functions of development board
  * Copyright (c) 2012-2021 Pervasive Displays Inc. All rights reserved.
  *  Authors: Pervasive Displays Inc.
  *  Redistribution and use in source and binary forms, with or without
  *  modification, are permitted provided that the following conditions
  *  are met:
  *  1. Redistributions of source code must retain the above copyright
  *     notice, this list of conditions and the following disclaimer.
  *  2. Redistributions in binary form must reproduce the above copyright
  *     notice, this list of conditions and the following disclaimer in
  *     the documentation and/or other materials provided with the
  *     distribution.
  *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
  *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
  *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
  *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
  *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifdef PROXY
   #include <stdint.h>
   extern "C" {
      void delay(int iTime);
      int EpdBusyWait(int State,int Timeout);
      void SendIndexData(uint8_t Cmd,const uint8_t *pData,uint32_t DataBytes);
      void SendIndexDataM(uint8_t Cmd,const uint8_t *pData,uint32_t DataBytes);
      void SendIndexDataS(uint8_t Cmd,const uint8_t *pData,uint32_t DataBytes);
      int EpdSetEnable(uint8_t Enable);
      int EpdSetReset(uint8_t Reset);
      int EpdSetDC(uint8_t DC);
      int EpdSetCS(uint8_t CS,uint8_t CS1);
      void EpdTestBWR_9_7();
   }
   #define _sendIndexData  SendIndexData
   #define _sendIndexDataM SendIndexDataM
   #define _sendIndexDataS SendIndexDataS
   #define LOW 0
   #define HIGH 1
#else
   #if defined(ENERGIA)
      #include "Energia.h"
   #else
      #include "Arduino.h"
   #endif
#endif

#include "EPD_Driver.h"

unsigned char reverse(unsigned char b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

// ---------- PUBLIC FUNCTIONS -----------

#ifdef PROXY
EPD_Driver::EPD_Driver(eScreen_EPD_t eScreen_EPD) 
#else
EPD_Driver::EPD_Driver(eScreen_EPD_t eScreen_EPD, pins_t board) 
#endif
{

#ifndef PROXY
   spi_basic = board;
#endif

   // Type
   pdi_cp = (uint16_t) eScreen_EPD;
   pdi_size = (uint16_t) (eScreen_EPD >> 8);

   uint16_t _screenSizeV = 0;
   uint16_t _screenSizeH = 0;
   uint16_t _screenDiagonal = 0;
   uint16_t _refreshTime = 0;

   switch (pdi_size) {
   case 0x96: // 9.69"

      _screenSizeV = 672; // v = wide size
      _screenSizeH = 960; // Actually, 960 = 480 x 2, h = small size
      _screenDiagonal = 970;
      _refreshTime = 45;
      break;

   case 0xB9: // 11.98"

      _screenSizeV = 768; // v = wide size
      _screenSizeH = 960; // Actually, 960 = 480 x 2, h = small size
      _screenDiagonal = 1220;
      _refreshTime = 45;
      break;

   default:

      break;
   }

   // Force conversion for two unit16_t multiplication into uint32_t.
   // Actually for 1 colour; BWR requires 2 pages.
   image_data_size = (uint32_t) _screenSizeV * (uint32_t) (_screenSizeH / 16);

#ifndef PROXY
   pinMode( spi_basic.panelBusy, INPUT );     //All Pins 0

   // pinMode( spi_basic.panelMOSI, OUTPUT );
   // pinMode( spi_basic.panelSCK, OUTPUT );

   pinMode( spi_basic.panelDC, OUTPUT );
   digitalWrite(spi_basic.panelDC, HIGH);
   pinMode( spi_basic.panelReset, OUTPUT );
   digitalWrite(spi_basic.panelReset, HIGH);

   pinMode( spi_basic.panelCS, OUTPUT );
   digitalWrite(spi_basic.panelCS, HIGH);
   pinMode( spi_basic.panelCSS, OUTPUT );
   digitalWrite(spi_basic.panelCSS, HIGH);


   if (spi_basic.panelON_EXT2 != 0xff) {
      pinMode( spi_basic.panelON_EXT2, OUTPUT );
      pinMode( spi_basic.panelSPI43_EXT2, OUTPUT );
      digitalWrite( spi_basic.panelON_EXT2, HIGH );    //PANEL_ON# = 1
      digitalWrite( spi_basic.panelSPI43_EXT2, LOW );
   }

   // pinMode(spi_basic.flashCS, OUTPUT);
   // digitalWrite(spi_basic.flashCS, HIGH);

   // SPI init
   // #ifndef SPI_CLOCK_MAX
   // #define SPI_CLOCK_MAX 16000000
   // #endif

#if defined(ENERGIA)
   {
      SPI.setDataMode(SPI_MODE0);
      SPI.setClockDivider(SPI_CLOCK_DIV32);
      // SPI.setClockDivider(16000000 / min(16000000, 4000000));
      SPI.setBitOrder(MSBFIRST);
      SPI.begin();
   }
#else
   {
      SPISettings _settingScreen;
      _settingScreen = {8000000, MSBFIRST, SPI_MODE0};
      SPI.begin();
      SPI.beginTransaction(_settingScreen);
   }
#endif
#endif
}

// CoG initialization function
//    Implements Tcon (COG) power-on and temperature input to COG
//    - INPUT:
//       - none but requires global variables on SPI pinout and config register data
void EPD_Driver::COG_initial() 
{
   //Initial COG
   uint8_t data4[] = { 0x7d};         
   _sendIndexData( 0x05, data4, 1 );
   delay(200);
   uint8_t data5[] = { 0x00};         
   _sendIndexData( 0x05, data5, 1 );
   delay( 10 );
   // uint8_t data6[] = { 0x3f};         
   // _sendIndexData( 0xc2, data6, 1 );
   // delay( 1 );
   uint8_t data7[] = { 0x80};         
   _sendIndexData( 0xd8, data7, 1 );    //MS_SYNC
   uint8_t data8[] = { 0x00};         
   _sendIndexData( 0xd6, data8, 1 );    //BVSS
   uint8_t data9[] = { 0x10};         
   _sendIndexData( 0xa7, data9, 1 );
   delay( 100 );  
   _sendIndexData( 0xa7, data5, 1 );
   delay( 100 );
   uint8_t data10[] = { 0x00, 0x11 };         
   _sendIndexData( 0x03, data10, 2 );    //OSC
   _sendIndexDataM( 0x44, data5, 1 );  //Master  
   uint8_t data11[] = { 0x80 };         
   _sendIndexDataM( 0x45, data11, 1 );    //Master 
   _sendIndexDataM( 0xa7, data9, 1 );   //Master
   delay( 100 );
   _sendIndexDataM( 0xa7, data5, 1 );    //Master 
   delay( 100 );
   uint8_t data12[] = { 0x06 };
   _sendIndexDataM( 0x44, data12, 1 );     //Master 
   uint8_t data13[] = { 0x82 };
   _sendIndexDataM( 0x45, data13, 1 );    //Temperature 0x82@25C
   _sendIndexDataM( 0xa7, data9, 1 );    //Master 
   delay( 100 );
   _sendIndexDataM( 0xa7, data5, 1 );     //Master 
   delay( 100 );

   _sendIndexDataS( 0x44, data5, 1 );     //Slave
   _sendIndexDataS( 0x45, data11, 1 );    //Slave
   _sendIndexDataS( 0xa7, data9, 1 );    //Slave
   delay( 100 );
   _sendIndexDataS( 0xa7, data5, 1 );     //Slave 
   delay( 100 );
   _sendIndexDataS( 0x44, data12, 1 );    //Slave 
   _sendIndexDataS( 0x45, data13, 1 );    //Temperature 0x82@25C for Slave
   _sendIndexDataS( 0xa7, data9, 1 );     //Slave
   delay( 100 );
   _sendIndexDataS( 0xa7, data5, 1 );     //Master 
   delay( 100 );

   uint8_t data14[] = { 0x25 };
   _sendIndexData( 0x60, data14, 1 );    //TCON
   uint8_t data15[] = { 0x01 };
   _sendIndexDataM( 0x61, data15, 1 );    //STV_DIR for Master
   // uint8_t data16[] = { 0x00 };
   // _sendIndexData( 0x01, data16, 1 );    //DCTL
   uint8_t data17[] = { 0x00 };
   _sendIndexData( 0x02, data17, 1 );    //VCOM
}

// CoG shutdown function
//    Shuts down the CoG and DC/DC circuit after all update functions
//    - INPUT:
//       - none but requires global variables on SPI pinout and config register data
void EPD_Driver::COG_powerOff() 
{
   uint8_t register_turnOff[] = {0x7f, 0x7d, 0x00};
   _sendIndexData(0x09, register_turnOff, 3);
   delay(200);

#ifndef PROXY
   while (digitalRead( spi_basic.panelBusy ) != HIGH);

   digitalWrite( spi_basic.panelDC, LOW );
   digitalWrite( spi_basic.panelCS, LOW );
   digitalWrite( spi_basic.panelBusy, LOW );
   delay( 150 );
   digitalWrite( spi_basic.panelReset, LOW );
#endif
}

// Global Update function
//    Implements global update functionality on either small/mid EPD
//    - INPUT:
//       - two image data (either BW and 0x00 or BW and BWR types)
void EPD_Driver::globalUpdate(const uint8_t * data1m, const uint8_t * data2m, const uint8_t * data1s, const uint8_t * data2s) {
   _reset(200, 20, 200, 200, 5);

   _sendImages(data1m, data2m, data1s, data2s);

   COG_initial();

   _DCDC_softStart();

   _displayRefresh();

   _DCDC_softShutdown();
}

// ---------- PROTECTED FUNCTIONS -----------

// SPI transfer function
//    Implements SPI transfer of index and data (consult user manual for EPD SPI process)
//    - INPUT:
//       - register address
//       - pointer to data char array
//       - length/size of data
#ifndef PROXY
void EPD_Driver::_sendIndexData( uint8_t index, const uint8_t *data, uint32_t len ) {
   digitalWrite( spi_basic.panelDC, LOW );      //DC Low
   digitalWrite( spi_basic.panelCS, LOW );      //CS Low
   digitalWrite( spi_basic.panelCSS, LOW );      //CS Low
   SPI.transfer(index);
   digitalWrite( spi_basic.panelCS, HIGH );     //CS High
   digitalWrite( spi_basic.panelCSS, HIGH );     //CS High
   digitalWrite( spi_basic.panelDC, HIGH );     //DC High
   digitalWrite( spi_basic.panelCS, LOW );      //CS Low
   digitalWrite( spi_basic.panelCSS, LOW );      //CS Low
   for (uint32_t i = 0; i < len; i++) {
      SPI.transfer(data[ i ]);
   }
   digitalWrite( spi_basic.panelCS, HIGH );     //CS High
   digitalWrite( spi_basic.panelCSS, HIGH );     //CS High
}
#endif   // PROXY

// EPD Screen refresh function
//    - INPUT:
//       - none but requires global variables on SPI pinout and config register data
void EPD_Driver::_displayRefresh() {
#ifdef PROXY
   EpdBusyWait(1,0);
#else
   while (digitalRead(spi_basic.panelBusy) != HIGH) {
      delay(100);
   }
#endif
   uint8_t data18[] = {0x3c};
   _sendIndexData(0x15, data18, 1); //Display Refresh
   delay(5);
}

// CoG driver power-on hard reset
//    - INPUT:
//       - none but requires global variables on SPI pinout and config register data
void EPD_Driver::_reset(uint32_t ms1, uint32_t ms2, uint32_t ms3, uint32_t ms4, uint32_t ms5) {
   // note: group delays into one array
#ifndef PROXY
   delay(ms1);
   digitalWrite(spi_basic.panelReset, HIGH); // RES# = 1
   delay(ms2);
   digitalWrite(spi_basic.panelReset, LOW);
   delay(ms3);
   digitalWrite(spi_basic.panelReset, HIGH);
   delay(ms4);
   digitalWrite(spi_basic.panelCS, HIGH); // CS# = 1
   digitalWrite(spi_basic.panelCSS, HIGH);
   delay(ms5);
#else
   EpdSetEnable(LOW);   // turn on power
   delay(ms1 + 100);
   EpdSetReset(HIGH);
   delay(ms2);
   EpdSetReset(LOW);
   delay(ms3);
   EpdSetReset(HIGH);
   delay(ms4);
   EpdSetCS(HIGH,HIGH);
   delay(ms5);
#endif
}

void EPD_Driver::_sendImages(const uint8_t * data1m, const uint8_t * data2m, const uint8_t * data1s, const uint8_t * data2s) {
   // Send image data
   uint8_t data1[] = { 0x00, 0x3b, 0x00, 0x00, 0x9f, 0x02 };
   if (pdi_size == 0xB9) data1[4] = 0xff;
   _sendIndexData( 0x13, data1, 6 );    //DUW for Both Master and Slave
   uint8_t data2[] = { 0x00, 0x3b, 0x00, 0xa9 }; 
   if (pdi_size == 0xB9) data2[3] = 0xc1;
   _sendIndexData( 0x90, data2, 4 );    //DRFW for Both Master and Slave
   uint8_t data3[] = { 0x3b, 0x00, 0x14 };         

   _sendIndexDataM( 0x12, data3, 3 );    //RAM_RW for Master   
   _sendIndexDataM( 0x10, data1m, image_data_size ); //First frame for Master
   _sendIndexDataM( 0x12, data3, 3 );    //RAM_RW for Master
   _sendIndexDataM( 0x11, data2m, image_data_size );   //Second frame for Master
   _sendIndexDataS( 0x12, data3, 3 );    //RAM_RW for Slave
   _sendIndexDataS( 0x10, data1s, image_data_size ); //First frame for Slave
   _sendIndexDataS( 0x12, data3, 3 );    //RAM_RW for Slave
   _sendIndexDataS( 0x11, data2s, image_data_size );   //Second frame 
}

// DC-DC soft-start command
//    Implemented after image data are uploaded to CoG
//    Specific to mid-sized EPDs only
void EPD_Driver::_DCDC_softStart() {
   //DCDC soft-start
   uint8_t  Index51_data[]={0x50,0x01,0x0a,0x01};
   _sendIndexData( 0x51, &Index51_data[0], 2 );
   uint8_t  Index09_data[]={0x1f,0x9f,0x7f,0xff};
   ///*
   for (int value=1;value<=4;value++) {
      _sendIndexData(0x09,&Index09_data[0],1);
      Index51_data[1]=value;
      _sendIndexData(0x51,&Index51_data[0],2);
      _sendIndexData(0x09,&Index09_data[1],1);
      delay(2);
   }
   //*
   for (int value=1;value<=10;value++) {
      _sendIndexData(0x09,&Index09_data[0],1);
      Index51_data[3]=value;
      _sendIndexData(0x51,&Index51_data[2],2);
      _sendIndexData(0x09,&Index09_data[1],1);
      delay(2);
   }
   for (int value=3;value<=10;value++) {
      _sendIndexData(0x09,&Index09_data[2],1);
      Index51_data[3]=value;
      _sendIndexData(0x51,&Index51_data[2],2);
      _sendIndexData(0x09,&Index09_data[3],1);
      delay(2);
   }
   for (int value=9;value>=2;value--) {
      _sendIndexData(0x09,&Index09_data[2],1);
      Index51_data[2]=value;
      _sendIndexData(0x51,&Index51_data[2],2);
      _sendIndexData(0x09,&Index09_data[3],1);
      delay(2);
   }
   _sendIndexData(0x09,&Index09_data[3],1);
   delay(10);
}

// DC-DC soft-shutdown command
//    Implemented after image data are uploaded to CoG
//    Specific to mid-sized EPDs only
void EPD_Driver::_DCDC_softShutdown() {
   // DC-DC off
#ifdef PROXY
   EpdBusyWait(1,0);
#else
   while (digitalRead(spi_basic.panelBusy) != HIGH) {
      delay(100);
   }
#endif
   uint8_t data19[] = {0x7f};
   _sendIndexData(0x09, data19, 1);
   uint8_t data20[] = {0x7d};
   _sendIndexData(0x05, data20, 1);
   uint8_t data55[] = {0x00};
   _sendIndexData(0x09, data55, 1);
   delay(200);

#ifdef PROXY
   EpdBusyWait(1,0);
#else
   while (digitalRead(spi_basic.panelBusy) != HIGH) {
      delay(100);
   }
   digitalWrite(spi_basic.panelDC, LOW);
   digitalWrite(spi_basic.panelCS, LOW);
   digitalWrite(spi_basic.panelReset, LOW);
   // digitalWrite(panelON_PIN, LOW); // PANEL_OFF# = 0

   digitalWrite(spi_basic.panelCS, HIGH); // CS# = 1
#endif
}

#ifndef PROXY
// SPI Master protocol setup
void EPD_Driver::_sendIndexDataM( uint8_t index, const uint8_t *data, uint32_t len ) 
{
   digitalWrite( spi_basic.panelCSS, HIGH );     //CSS slave High
   digitalWrite( spi_basic.panelDC, LOW );      //DC Low
   digitalWrite( spi_basic.panelCS, LOW );      //CS Low
   SPI.transfer( index );
   digitalWrite( spi_basic.panelCS, HIGH );     //CS High
   digitalWrite( spi_basic.panelDC, HIGH );     //DC High
   digitalWrite( spi_basic.panelCS, LOW );      //CS Low
   for (int i = 0; i < len; i++) SPI.transfer( data[ i ] );
   digitalWrite( spi_basic.panelCS, HIGH );     //CS High
}

// SPI Slave protocol setup
void EPD_Driver::_sendIndexDataS( uint8_t index, const uint8_t *data, uint32_t len ) {
   digitalWrite( spi_basic.panelCS, HIGH );     //CS Master High
   digitalWrite( spi_basic.panelDC, LOW );      //DC Low
   digitalWrite( spi_basic.panelCSS, LOW );      //CS slave Low
   SPI.transfer( index );
   digitalWrite( spi_basic.panelCSS, HIGH );     //CS slave High
   digitalWrite( spi_basic.panelDC, HIGH );     //DC High
   digitalWrite( spi_basic.panelCSS, LOW );      //CS slave Low
   for (int i = 0; i < len; i++) SPI.transfer( data[ i ] );
   digitalWrite( spi_basic.panelCSS, HIGH );     //CS slave High
}
#endif   // PROXY
