/*
 *  Copyright (C) 2024  Skip Hansen
 * 
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms and conditions of the GNU General Public License,
 *  version 2, as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 */
// This serial link byte stuffing protocol is inspired by the Consistent 
// Overhead Byte Stuffing (COBS) the following additions:
// 1. Mandatory 0x00 byte at the beginning and end of every frame. 
//    This 1 allows us to multiplex ASCII logging with framed messages.
// 2. Addition of a 16 bit CRC at the end of the frame.
//    This allows us to detect corrupted data
// 
// The size of the Rx and Tx buffers is determined by the maximum message
// length (MaxMsgLen).
// 
// The Rx buffer must be MaxMsgLen plus 2 bytes for the CRC.
// 
// The Tx buffer must be the MaxMsgLen messages length plus 2 bytes for the 
// frame delimiters + 2 byte for the CRC plus (1 + MaxMsgLen / 254) overhead
// bytes.
// 
// If the serial link is half duplex the same buffer can be used for both
// receive and transmit.
// 
// The only external dependency is SerialFrameIO_SendByte which simply
// sends a byte on the serial line.
// 

#include <stdint.h>
#include "CobsFraming.h"

#ifdef COBS_TEST
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "logging.h"

uint8_t TestData[1000];
uint8_t Encoded[2000];
uint8_t CobsBuf[2000];
int gTxLen;
#endif

// #define NO_CRC

struct {
   uint8_t *RxBuf;
   int MaxMsgLen;
} gCOBS;

static void UpdateCRC(uint8_t data,uint16_t *pCrc)
{
   int i;
   uint16_t res;

//   printf("Updating CRC with 0x%02x 0x%04x -> ",data,*pCrc);

   res = *pCrc;
   res ^= (unsigned int) data; 
   for(i = 0; i < 8 ; i++) {
      if(res & 0x0001) {
         res >>= 1;
         res ^= 0xA001;
      }
      else {
         res >>= 1;
      }
   }
   *pCrc = res;   
//   printf("0x%04x\n",*pCrc);
}

// The Tx buffer must be the MaxMsgLen messages length plus 2 bytes for the 
// frame delimiters + 2 byte for the CRC plus 1 + (MaxMsgLen / 254) overhead
// bytes.
static int CalcOverhead(int MaxMsgLen)
{
   return 2 + 2 + 1 + (MaxMsgLen / 254);
}

// public functions

int SerialFrameIO_CalcBufLen(int MaxMsgLen)
{
   gCOBS.MaxMsgLen = MaxMsgLen;
   return MaxMsgLen + CalcOverhead(MaxMsgLen);
}


// Return the maximum message length based on the size of the buffer provided.
int SerialFrameIO_Init(uint8_t *Buf,int BufSize)
{
   int OverHead = 2 + 2 + 1;
   OverHead += (BufSize - OverHead) / 254;

   gCOBS.MaxMsgLen = BufSize - OverHead;
   gCOBS.RxBuf = Buf;

   return gCOBS.MaxMsgLen;
}

// Parse a byte of data
// Returns
//     > 0: length of valid message that was received
//    == 0: Didn't consume character (i.e. not message data)
//     < 0: Consumed data
// 
int SerialFrameIO_ParseByte(uint8_t RxByte)
{
   static int RxCount = -1;
   static int RunLen;   // number of bytes to EOF or next 0 data byte
   static uint16_t Crc;
   int Ret = 0;  // Assume we are not going to consume the byte

   if(RxByte == 0) {
   // Start or end of frame (or data error)
      Ret = -1;
      if(RxCount <= 0) {
      // Start of a new frame
         RxCount = 0;
         RunLen = 0;
         Crc = 0xffff;
      }
      else if(Crc == 0) {
      // Got a good frame
         Ret = RxCount - 2;   // Adjust for CRC
         RxCount = -1;
      }
   }
   else if(RxCount >= 0) {
   // Decoding COBS frame
      Ret = -1;
      if(RunLen == 0) {
      // get overhead byte
         RunLen = RxByte;
      }
      else {
         RunLen--;
         if(RunLen == 0) {
         // update RunLen
            RunLen = RxByte;
            RxByte = 0; // Data is 0
         }
         if(RxCount >= gCOBS.MaxMsgLen) {
         // this message is too big, toss it!
            RxCount = -1;
         }
         else {
            gCOBS.RxBuf[RxCount++] = RxByte;
            UpdateCRC(RxByte,&Crc);
         }
      }
   }

   return Ret;
}

static void SendCobsData(uint8_t *pData,int MsgLen)
{
   if(MsgLen > 0) {
      uint8_t *pTx = pData;
      uint8_t RunLen = 1;

      SerialFrameIO_SendByte(0); // Start of frame

      while(MsgLen-- > 0) {
         if(*pData++ == 0) {
         // Send RunLen bytes of data
            SerialFrameIO_SendByte(RunLen--);
            while(RunLen-- != 0) {
               SerialFrameIO_SendByte(*pTx++);
            }
            RunLen = 1;
            pTx = pData;
         }
         else {
            RunLen++;
         }

         if(RunLen == 0) {
         // RunLen is max, flush 254 bytes
            RunLen = 255;
            SerialFrameIO_SendByte(RunLen--);
            while(RunLen-- != 0) {
               SerialFrameIO_SendByte(*pTx++);
            }
            RunLen = 0;
         }
      }
      if(RunLen > 0) {
         SerialFrameIO_SendByte(RunLen--);
         while(RunLen-- != 0) {
            SerialFrameIO_SendByte(*pTx++);
         }
      }
      SerialFrameIO_SendByte(0); // End of frame
   }
}

// MsgLen is total bytes to send
// The buffer pointed to by Msg MUST have room for the 2 byte CRC which 
// will be appended to the data
void SerialFrameIO_SendMsg(uint8_t *Msg,int MsgLen)
{
   uint8_t *pData;
   int CrcCount = MsgLen;
   uint16_t Crc = 0xffff;

#ifndef NO_CRC
   if(MsgLen > 0) {
      pData = Msg;
      while(CrcCount-- > 0) {
         UpdateCRC(*pData++,&Crc);
      }
      *pData = (uint8_t) (Crc & 0xff);
      pData++;
      *pData = (uint8_t) ((Crc >> 8) & 0xff);
      SendCobsData(Msg,MsgLen + 2);
   }
#else
   SendCobsData(Msg,MsgLen);
#endif
}

#ifdef COBS_TEST

int EncodeAndDecodeTest(uint8_t *TestData,int TestMsgLen)
{
   int Ready;
   int ErrLine = 0;
   int i;

   do {
      printf("Test data (%d bytes):\n",TestMsgLen);
      DumpHex(TestData,TestMsgLen);

      gTxLen = 0;
      SerialFrameIO_SendMsg(TestData,TestMsgLen);

      printf("Encoded COBS data (%d bytes):\n",gTxLen);
      DumpHex(Encoded,gTxLen);
      break;

      for(i = 0; i < gTxLen; i++) {
         Ready = SerialFrameIO_ParseByte(Encoded[i]);
         if(Ready == 0) {
            printf("Didn't consume data at index %d\n",i);
            ErrLine = __LINE__;
            break;
         }
         else if(Ready > 0) {
            break;
         }
      }

      if(ErrLine != 0) {
         break;
      }
      if(Ready != TestMsgLen) {
         printf("Incorrect Rx msg len, sent %d, got %d\n",
                TestMsgLen,Ready);
         ErrLine = __LINE__;
         break;
      }

#ifndef NO_CRC
      for(i = 0; i < TestMsgLen; i++) {
         if(TestData[i] != gCOBS.RxBuf[i]) {
            printf("Received data comparison failed at offset %d, 0x%x != 0x%x\n",
                   i,TestData[i],gCOBS.RxBuf[i]);
            DumpHex(gCOBS.RxBuf,TestMsgLen);
            ErrLine = __LINE__;
            break;
         }
      }
#endif

   } while(0);

   return ErrLine;
}

int main(int argc, char* argv[])
{
   int i;
   int MaxMsgLen;
   int ErrLine = 0;
   uint16_t j;

   do {
      SerialFrameIO_Init(CobsBuf,sizeof(CobsBuf));
      MaxMsgLen = gCOBS.MaxMsgLen;

      printf("MaxMsgLen %d\n",MaxMsgLen);
      printf("CalcOverhead(MaxMsgLen) %d\n",CalcOverhead(MaxMsgLen));
      if(MaxMsgLen < sizeof(TestData)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      if(EncodeAndDecodeTest(TestData,1)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      if(EncodeAndDecodeTest(TestData,2)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      TestData[1] = 0x11;
      if(EncodeAndDecodeTest(TestData,3)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      TestData[0] = 0x11;
      TestData[1] = 0x22;

      if(EncodeAndDecodeTest(TestData,3)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      TestData[0] = 0x11;
      TestData[1] = 0x22;
      TestData[2] = 0x33;
      TestData[3] = 0x44;

      if(EncodeAndDecodeTest(TestData,4)) {
         ErrLine = __LINE__;
         break;
      }

      memset(TestData,0,sizeof(TestData));
      TestData[0] = 0x11;

      if(EncodeAndDecodeTest(TestData,4)) {
         ErrLine = __LINE__;
         break;
      }

      i = 0;
      for(j = 1; j <= 0xfe; j++) {
         TestData[i++] = (uint8_t) (j & 0xff);
      }

      if(EncodeAndDecodeTest(TestData,i)) {
         ErrLine = __LINE__;
         break;
      }

      i = 0;
      for(j = 0; j <= 0xfe; j++) {
         TestData[i++] = (uint8_t) (j & 0xff);
      }

      if(EncodeAndDecodeTest(TestData,i)) {
         ErrLine = __LINE__;
         break;
      }

      i = 0;
      for(j = 1; j <= 0xff; j++) {
         TestData[i++] = (uint8_t) (j & 0xff);
      }

      if(EncodeAndDecodeTest(TestData,i)) {
         ErrLine = __LINE__;
         break;
      }

      i = 0;
      for(j = 2; j <= 0x100; j++) {
         TestData[i++] = (uint8_t) (j & 0xff);
      }

      if(EncodeAndDecodeTest(TestData,i)) {
         ErrLine = __LINE__;
         break;
      }


      i = 0;
      for(j = 3; j <= 0x101; j++) {
         TestData[i++] = (uint8_t) (j & 0xff);
      }

      if(EncodeAndDecodeTest(TestData,i)) {
         ErrLine = __LINE__;
         break;
      }

      printf("RAND_MAX %d\n",RAND_MAX);

      for(i = 0; i < 1000; i++) {
         int TestMsgLen = rand();
         printf("rand returned %d ",TestMsgLen);
         TestMsgLen %= MaxMsgLen;
         printf("TestMsgLen %d\n",TestMsgLen);
         if(TestMsgLen == 0) {
            TestMsgLen++;
         }
         for(j = 0; j < TestMsgLen; j++) {
            TestData[j] = (uint8_t) (rand() & 0xff);
         }
         if(EncodeAndDecodeTest(TestData,i)) {
            ErrLine = __LINE__;
            break;
         }
      }
   } while(0);

   if(ErrLine != 0) {
      printf("Tests failed at line %d\n",ErrLine);
   }
   else {
      printf("All tests passed\n");
   }
}

void _log(const char *fmt,...)
{
   va_list args;
   va_start(args,fmt);

   vprintf(fmt,args);
   va_end(args);
}

void SerialFrameIO_SendByte(uint8_t TxByte)
{
   Encoded[gTxLen++] = TxByte;
}

#endif

