/*
 *  Copyright (C) 2023  Skip Hansen
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
// This serial link byte stuffing protocol is inspired by SLIP and PPP but 
// isn't either.
// 
// "Protocol": <flag sequence> <binary data> <crc16> <flag sequence>
// flag sequence: <special byte>, <flag byte>
// Any occurrence of special byte in <binary data> is "escaped" by sending
//
// The serial channel must be 8 bit transparent, i.e. no bytes
// can have a special meaning (xoff, xon, etc)
// 
// The maximum frame length is specified by the caller.
// The only external dependency is SerialFrameIO_SendByte which simply
// sends a byte on the serial line.
// 

#include <stdint.h>
#include "SerialFraming.h"

#define SPECIAL   0xaa
#define FLAG      0x01

typedef enum {
   FLAG_WAIT = 1,    // waiting for <SPECIAL> <FLAG> to start new frame
   GET_DATA,         // have <SPECIAL><FLAG>, getting data
   SPECIAL_ESCAPE,   // Last byte was an <SPECIAL>, waiting for next byte
} ReceiveState;

#define RX_BUF_LEN   80
static uint8_t *gInputBuf;
static int gInBufSize;

static void UpdateCRC(uint8_t uint8_t_data,uint16_t *pCrc)
{
   int i;
   uint16_t res;

   res = *pCrc;
   res ^= (unsigned int) uint8_t_data; 
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
}

static void EscapedSend(uint8_t Byte)
{
   if(Byte == SPECIAL) {
      SerialFrameIO_SendByte(SPECIAL);
   }
   SerialFrameIO_SendByte(Byte);
}

// public functions
void SerialFrameIO_Init(uint8_t *RxBuf,int RxBufSize)
{
   gInputBuf = RxBuf;
   gInBufSize = RxBufSize;
}

// Parse a byte of data
// Returns
//     > 0: length of valid message that was received
//    == 0: Didn't consume character (i.e. not message data)
//     < 0: Consumed data
// 
int SerialFrameIO_ParseByte(uint8_t RxByte)
{
   static ReceiveState ParseState = FLAG_WAIT;
   static ReceiveState LastState;
   static int RxCount;
   static uint16_t Crc;
   int Ret = 0;  // Assume we are not going to consume the byte

   switch(ParseState) {
      case FLAG_WAIT:
         if(RxByte == SPECIAL) {
            LastState = ParseState;
            ParseState = SPECIAL_ESCAPE;
            Ret = -1;
         }
         break;

      case GET_DATA:
         Ret = -1;
         if(RxByte != SPECIAL) {
            gInputBuf[RxCount++] = RxByte;
            UpdateCRC(RxByte,&Crc);
         }
         else {
         // It's a SPECIAL byte, next byte tells if it's data or end of frame 
            LastState = ParseState;
            ParseState = SPECIAL_ESCAPE;
         }
         break;

      case SPECIAL_ESCAPE:
      // The last character was an SPECIAL
         Ret = -1;
         if(RxByte == FLAG) {
         // frame boundary
            if(RxCount == 0) {
            // Start of frame
               ParseState = GET_DATA;
            }
            else if(Crc == 0 && RxCount > 1) {
            // Good frame
               ParseState = FLAG_WAIT;
               Ret = RxCount - 2;   // Adjust for CRC
            }
            RxCount = 0;
            Crc = 0xffff;
         }
         else if(RxByte == SPECIAL && LastState == GET_DATA) {
         // Escaped SPECIAL, stay in GET_DATA state
            gInputBuf[RxCount++] = SPECIAL;
            UpdateCRC(RxByte,&Crc);
            ParseState = GET_DATA;
         }
         else {
         // Bogus, got back to waiting for the start of a new frame
            ParseState = FLAG_WAIT;
         }
         break;
   }

   if(RxCount >= gInBufSize) {
   // this message is too big, toss it!
      ParseState = FLAG_WAIT;
   }

   return Ret;
}

// MsgLen is total bytes to send including Type, Data and *pData
static void SendInternal(uint8_t Type,uint8_t Data,uint8_t *pData,int MsgLen)
{
   if(MsgLen-- > 0) {
      uint16_t Crc = 0xffff;

      SerialFrameIO_SendByte(SPECIAL);
      SerialFrameIO_SendByte(FLAG);
      EscapedSend(Type);
      UpdateCRC(Type,&Crc);
      if(MsgLen > 0) {
         MsgLen--;
         EscapedSend(Data);
         UpdateCRC(Data,&Crc);
      }
      while(MsgLen-- > 0) {
         EscapedSend(*pData);
         UpdateCRC(*pData++,&Crc);
      }
      EscapedSend((uint8_t) (Crc & 0xff));
      EscapedSend((uint8_t) ((Crc >> 8) & 0xff));
      SerialFrameIO_SendByte(SPECIAL);
      SerialFrameIO_SendByte(FLAG);
   }
}

// MsgLen is total bytes to send
void SerialFrameIO_SendMsg(uint8_t *Msg,int MsgLen)
{
   uint8_t Type;
   uint8_t Data = 0;

   if(MsgLen > 0) {
      Type = Msg[0];
      if(MsgLen >= 2) {
         Data = Msg[1];
      }
      SendInternal(Type,Data,&Msg[2],MsgLen);
   }
}

// DataLen is total bytes of payload to send
void SerialFrameIO_SendCmd(uint8_t Cmd,uint8_t *Data,int DataLen)
{
   uint8_t FirstByte = 0;

   if(DataLen > 1) {
      FirstByte = Data[0];
   }
   SendInternal(Cmd,FirstByte,&Data[1],DataLen+1);
}

// DataLen is total bytes of payload to send
void SerialFrameIO_SendResp(uint8_t Cmd,uint8_t Err,uint8_t *Data,int DataLen)
{
   SendInternal(Cmd,Err,Data,DataLen+2);
}


