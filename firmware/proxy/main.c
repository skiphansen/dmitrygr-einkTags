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

#define __packed
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "asmUtil.h"
#include "board.h"
#include "timer.h"
#include "printf.h"
#include "cpu.h"
#include "wdt.h"
#include "SerialFraming.h"
#include "cc1110-ext.h"
#include "proxy_msgs.h"

#define xstr(s) str(s)
#define str(s) #s
#define LOG pr

#define NUM_RF_REGS  0x3d

typedef union {
   int IntValue;
   uint32_t Uint32Value;
   int32_t Int32Value;
   uint16_t Adr;
   uint32_t Adr32;
   __xdata uint8_t *pXdata; 
   uint8_t Bytes[4];
   uint8_t Uint8Value;
} CastUnion;

volatile __xdata uint8_t gRfStatus;
uint8_t __xdata gRxBuf[130];
int gRxMsgLen;
const char gBuildType[] = xstr(BUILD_TYPE);

void HandleMsg(void);
void RxMode(void);
void TxMode(void);
void IdleMode(void);
void startRX(void);

void main(void)
{
   clockingAndIntsInit();
   timerInit();
   boardInit();
   SerialFrameIO_Init(gRxBuf,sizeof(gRxBuf));

   u1setUartMode();
   LOG("proxy_%s v0.01, compiled " __DATE__ " " __TIME__ "\n",gBuildType);
   u1setEepromMode();
   if (!eepromInit()) {
      u1setUartMode();
      LOG("failed to init eeprom\n");
   }
   u1setUartMode();
   irqsOn();

   while(true) {
      uint8_t Byte;
      if(U1CSR & 0x04) {
         Byte = U1DBUF;
         gRxMsgLen = SerialFrameIO_ParseByte(Byte);
         if(gRxMsgLen > 0) {
         // Message avaliable
            HandleMsg();
            LOG("Back from HandleMsg\n");
         }
      }
   }
}

void SerialFrameIO_SendByte(uint8_t Byte)
{
   u1byte(Byte);
}

#define CMD_RESP  0x80
// commands <CmdByte> <command data>
// respones <CmdByte | 0x80> <Rcode> <response data>
void HandleMsg()
{
   int MsgLen = 2;
   CastUnion uCast0;
   CastUnion uCast1;
   CastUnion *pResponse = (CastUnion *) &gRxBuf[2];

// Assume that the first two arguments are 16 bits
   uCast0.Bytes[0] = gRxBuf[1];
   uCast0.Bytes[1] = gRxBuf[2];
   uCast0.Bytes[2] = uCast0.Bytes[3] = 0;
   uCast1.Bytes[0] = gRxBuf[3];
   uCast1.Bytes[1] = gRxBuf[4];
   uCast1.Bytes[2] = uCast1.Bytes[3] = 0;
#if 0
   {
      LOG("gRxBuf:");
      for(MsgLen = 0; MsgLen < 16; MsgLen++) {
         LOG(" %02x",gRxBuf[MsgLen]);
      }
      LOG("\n");
      MsgLen = 2;
   }
#endif

// default to reusing gRxBuf for the response 
   gRxBuf[0] = gRxBuf[0] | CMD_RESP;
   gRxBuf[1] = CMD_ERR_NONE;

   switch(gRxBuf[0] & ~CMD_RESP) {
      case CMD_PING:
         LOG("Got ping\n");
         break;

      case CMD_EEPROM_RD:
         if(uCast0.IntValue > sizeof(gRxBuf) - 2) {
            gRxBuf[1] = CMD_ERR_BUF_OVFL;
            break;
         }
         u1setEepromMode();
         uCast1.Bytes[2] = gRxBuf[5];
         eepromRead(uCast1.Adr32,(void __xdata *) &gRxBuf[2],uCast0.IntValue);
         u1setUartMode();
         MsgLen += uCast0.IntValue;
         break;

      case CMD_EEPROM_LEN:
         pResponse->Uint32Value = eepromGetSize();
         MsgLen += sizeof(uCast1.Uint32Value);
         break;

      case CMD_COMM_BUF_LEN:
         pResponse->IntValue = sizeof(gRxBuf);
         MsgLen += sizeof(uCast1.IntValue);
         break;

      case CMD_BOARD_TYPE:
         MsgLen += strlen(gBuildType);
         memcpy(&gRxBuf[2],gBuildType,MsgLen-2);
         break;

      case CMD_PEEK:
         if(uCast0.IntValue > sizeof(gRxBuf) - 2) {
            gRxBuf[1] = CMD_ERR_BUF_OVFL;
            break;
         }
         memcpy(&gRxBuf[2],uCast1.pXdata,uCast0.IntValue);
         MsgLen += uCast0.IntValue;
         break;

// <reg adr>,<reg value> ...
// (Only the LSB of the register is sent)
      case CMD_SET_RF_REGS:
         gRxBuf[1] = uCast0.Bytes[0];  // restore the first byte of data
         MsgLen = 1;  // one byte already consumed (command)
         uCast0.pXdata = &SYNC1;
         uCast1.pXdata = &gRxBuf[1];

         while(MsgLen < gRxMsgLen) {
            uCast0.Bytes[0] = *uCast1.pXdata++;   // set LSB of reg adr
            *uCast0.pXdata = *uCast1.pXdata++;
            MsgLen += 2;
         }
         gRxBuf[1] = CMD_ERR_NONE;
         MsgLen = 2;
         break;

      case CMD_GET_RF_REGS:
         memcpy(&gRxBuf[2],&SYNC1,NUM_RF_REGS);
         MsgLen += NUM_RF_REGS;
         break;

      case CMD_SET_RF_MODE:
         switch(uCast0.Bytes[0]) {
            case RFST_SRX:
               RxMode();
               break;

            case RFST_SIDLE:
               IdleMode();
               break;

            case RFST_STX:
               TxMode();
               LOG("Back from TxMode\n");
               break;

            default:
               gRxBuf[1] = CMD_ERR_INVALID_ARG;
               break;
         }
         break;

#if 0
      case CMD_EEPROM_WR:
         break;


      case CMD_POKE:
         loop =  *ptr++;
         loop += *ptr++ << 8;
         ep5.dptr = (__xdata u8*) loop;

         loop = ep5.OUTlen - 2;

         for(;loop>0;loop--) {
            *ep5.dptr++ = *ptr++;
         }

         //if (ep5.OUTbytesleft == 0)
         txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata u8*)&(ep5.OUTbytesleft));
         break;

      case CMD_POKE_REG:
         if(!(ep5.flags & EP_OUTBUF_CONTINUED)) {
            loop =  *ptr++;
            loop += *ptr++ << 8;
            ep5.dptr = (__xdata u8*) loop;
         }
         // FIXME: do we want to DMA here?

         loop = ep5.OUTbytesleft;
         if(loop > EP5OUT_MAX_PACKET_SIZE) {
            loop = EP5OUT_MAX_PACKET_SIZE;
         }

         ep5.OUTbytesleft -= loop;
         //debughex16(loop);

         for(;loop>0;loop--) {
            *ep5.dptr++ = *ptr++;
         }

         txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata u8*)&(ep5.OUTbytesleft));

         break;

      case CMD_STATUS:
         txdata(ep5.OUTapp, ep5.OUTcmd, 13, (__xdata u8*)"UNIMPLEMENTED");
         // unimplemented
         break;

      case CMD_GET_CLOCK:
         txdata(ep5.OUTapp, ep5.OUTcmd, 4, (__xdata u8*)clock);
         break;

#endif
      case CMD_RESET:
         wdtDeviceReset();
         break;
#if 0
      case CMD_CLEAR_CODES:
         lastCode[0] = 0;
         lastCode[1] = 0;
         //txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);   // FIXME: need to reorient all these to return LCE_NO_ERROR unless error.
         appReturn(2, ptr);
         break;
#endif

      default:
         LOG("Unknown command 0x%x ignored\n",gRxBuf[0]);
         gRxBuf[1] = CMD_ERR_INVALID_ARG;
         break;
   }

   if(MsgLen != 0) {
   // Send reply in gRxBuf
      SerialFrameIO_SendMsg(gRxBuf,MsgLen);
   }
}

void RxMode()
{
   if(gRfStatus != RFST_SRX) {
      LOG("Set SRX\n");
      MCSM1 &= 0xf0;
      MCSM1 |= 0x0f;
      gRfStatus = RFST_SRX;
      startRX();
   }
}


// enter TX mode
void TxMode()
{
   if(gRfStatus != RFST_STX) {
      LOG("Set STX RFIM %d\n",RFIM);
      MCSM1 &= 0xf0;
      MCSM1 |= 0x0a;

      gRfStatus = RFST_STX;
      RFST = RFST_STX;
      while(MARCSTATE != MARC_STATE_TX);
      LOG("returning\n");
   }
}

// enter IDLE mode  (this is significant!  don't do lightly or quickly!)
void IdleMode()
{
   if(gRfStatus != RFST_SIDLE) {
      LOG("Set SIDLE\n");
      MCSM1 &= 0xf0;
      RFIM &= ~RFIF_IRQ_DONE;
      RFST = RFST_SIDLE; 
      while(MARCSTATE != MARC_STATE_IDLE);

#ifdef RFDMA
      DMAARM |= (0x80 | DMAARM0);                 // ABORT anything on DMA 0
      DMAIRQ &= ~1;
#endif

      S1CON &= ~(S1CON_RFIF_0|S1CON_RFIF_1);  // clear RFIF interrupts
      RFIF &= ~RFIF_IRQ_DONE;
      gRfStatus = RFST_SIDLE;
      // FIXME: make this also adjust radio register settings for "return to" state?
   }
}

// prepare for RF RX
void startRX()
{
#if 0
    /* If DMA transfer, disable rxtx interrupt */
#ifdef RFDMA
    RFTXRXIE = 0;
#else
    RFTXRXIE = 1;
#endif

    /* Clear rx buffer */
    memset(rfrxbuf,0,BUFFER_SIZE);

    /* Set both byte counters to zero */
    rfRxCounter[FIRST_BUFFER] = 0;
    rfRxCounter[SECOND_BUFFER] = 0;

    /*
    * Process flags, set first flag to false in order to let the ISR write bytes into the buffer,
    *  The second buffer should flag processed on initialize because it is empty.
    */
    rfRxProcessed[FIRST_BUFFER] = RX_UNPROCESSED;
    rfRxProcessed[SECOND_BUFFER] = RX_PROCESSED;

    /* Set first buffer as current buffer */
    rfRxCurrentBuffer = 0;

    S1CON &= ~(S1CON_RFIF_0|S1CON_RFIF_1);
    RFIF &= ~RFIF_IRQ_DONE;

#ifdef RFDMA
    {
        rfDMA.srcAddrH = ((u16)&X_RFD)>>8;
        rfDMA.srcAddrL = ((u16)&X_RFD)&0xff;
        rfDMA.destAddrH = ((u16)&rfrxbuf[rfRxCurrentBuffer])>>8;
        rfDMA.destAddrL = ((u16)&rfrxbuf[rfRxCurrentBuffer])&0xff;
        rfDMA.lenH = 0;
        rfDMA.vlen = 0;
        rfDMA.lenL = 12;
        rfDMA.trig = 19;
        rfDMA.tMode = 0;
        rfDMA.wordSize = 0;
        rfDMA.priority = 1;
        rfDMA.m8 = 0;
        rfDMA.irqMask = 0;
        rfDMA.srcInc = 0;
        rfDMA.destInc = 1;

        DMA0CFGH = ((u16)(&rfDMA))>>8;
        DMA0CFGL = ((u16)(&rfDMA))&0xff;
        
        DMAIRQ &= ~DMAARM0;
        DMAARM |= (0x80 | DMAARM0);
        NOP(); NOP(); NOP(); NOP();
        NOP(); NOP(); NOP(); NOP();
        DMAARM = DMAARM0;
        NOP(); NOP(); NOP(); NOP();
        NOP(); NOP(); NOP(); NOP();
    }
#endif

    RFRX;

    RFIM |= RFIF_IRQ_DONE;
#endif
}
