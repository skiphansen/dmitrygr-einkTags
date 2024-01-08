/* 
  This code was derived code downloaded from  https://dmitry.gr/?r=05.Projects&proj=29.%20eInk%20Price%20Tags
 
  Dimitry didn't include a LICENSE file or copyright headers in the source
  code but the web page containing the ZIP file I downloaded says:
 
  "The license is simple: This code/data/waveforms are free for use in hobby
  and other non-commercial products."

   For commercial use, contact him (licensing@dmitry.gr)
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
#include "proxy_msgs.h"
#include "radio.h"

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

#define MAX_FRAME_IO_LEN      130
uint8_t __xdata gRxBuf[MAX_FRAME_IO_LEN];
uint8_t __xdata gTxBuf[MAX_FRAME_IO_LEN*2];
int gRxMsgLen;
int gTxMsgLen;

const char gBuildType[] = xstr(BUILD_TYPE);

#define UART1_RX_RING_LEN  64
volatile uint8_t __xdata gUart1RxBuf[UART1_RX_RING_LEN];
volatile uint8_t __xdata gUart1RxWr;
volatile uint8_t __xdata gUart1RxRd;
volatile __bit gRxOn;


void HandleMsg(void);
void RxMode(void);
void TxMode(void);
void IdleMode(void);
void startRX(void);
void TryRx(void);

void main(void)
{
   clockingAndIntsInit();
   timerInit();
   boardInit();
   SerialFrameIO_Init(gRxBuf,sizeof(gRxBuf));

   u1setUartMode();
   LOG("proxy_%s v0.01, compiled " __DATE__ " " __TIME__ "\n",gBuildType);
   u1setEepromMode();
   if(!eepromInit()) {
      u1setUartMode();
      LOG("failed to init eeprom\n");
   }
   u1setUartMode();
   URX1IE = 1;
   // RFTXRXIE = 1;

   irqsOn();
   radioInit();

// Forever loop
   while(true) {
      uint8_t Byte;
      while(gUart1RxWr != gUart1RxRd) {
         Byte = gUart1RxBuf[gUart1RxRd++];
         if(gUart1RxRd == UART1_RX_RING_LEN) {
            gUart1RxRd = 0;
         }
         gRxMsgLen = SerialFrameIO_ParseByte(Byte);
         if(gRxMsgLen > 0) {
         // Message avaliable
            HandleMsg();
         }
      }
      if(gRfStatus == RFST_SRX) {
         TryRx();
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
         // purge the receiver of any data received in SPI mode
         gUart1RxRd = gUart1RxWr;
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
         ep5.dptr = (__xdata uint8_t*) loop;

         loop = ep5.OUTlen - 2;

         for(;loop>0;loop--) {
            *ep5.dptr++ = *ptr++;
         }

         //if (ep5.OUTbytesleft == 0)
         txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata uint8_t*)&(ep5.OUTbytesleft));
         break;

      case CMD_POKE_REG:
         if(!(ep5.flags & EP_OUTBUF_CONTINUED)) {
            loop =  *ptr++;
            loop += *ptr++ << 8;
            ep5.dptr = (__xdata uint8_t*) loop;
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

         txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata uint8_t*)&(ep5.OUTbytesleft));

         break;

      case CMD_STATUS:
         txdata(ep5.OUTapp, ep5.OUTcmd, 13, (__xdata uint8_t*)"UNIMPLEMENTED");
         // unimplemented
         break;

      case CMD_GET_CLOCK:
         txdata(ep5.OUTapp, ep5.OUTcmd, 4, (__xdata uint8_t*)clock);
         break;

#endif
      case CMD_RESET:
         if(MARCSTATE != MARC_STATE_IDLE) {
            // The watchdog won't timeout while transmitting !!!
            IdleMode(); 
         }
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
      LOG("startRX\n");
      startRX();
   }
}


// enter TX mode
void TxMode()
{
   if(gRfStatus != RFST_STX) {
      LOG("Set STX\n");
      MCSM1 &= 0xf0;
      MCSM1 |= 0x0a;

      gRfStatus = RFST_STX;
      RFST = RFST_STX;
      while(MARCSTATE != MARC_STATE_TX);
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

      DMAARM |= (0x80 | DMAARM0);                 // ABORT anything on DMA 0
      DMAIRQ &= ~1;

      S1CON &= ~(S1CON_RFIF_0|S1CON_RFIF_1);  // clear RFIF interrupts
      RFIF &= ~RFIF_IRQ_DONE;
      gRfStatus = RFST_SIDLE;
      // FIXME: make this also adjust radio register settings for "return to" state?
   }
}

// prepare for RF RX
void startRX()
{
   if(gRfStatus != RFST_SRX) {
      gRfStatus = RFST_SRX;
      radioRxEnable(true,false);
   }
}

void rx1_isr(void) __interrupt (URX1_VECTOR)
{
   gUart1RxBuf[gUart1RxWr++] = U1DBUF;
   if(gUart1RxWr == UART1_RX_RING_LEN) {
   // wrap
      gUart1RxWr = 0;
   }
}

void TryRx()
{
   uint8_t __xdata * __xdata Buf;
   int8_t Len;
   uint8_t __xdata Lqi = 0;
   int8_t __xdata RSSI = 0;

   if((Len = radioRxDequeuePktGet(&Buf,&Lqi,&RSSI)) > 0) {
   // Send it
      SerialFrameIO_SendResp(CMD_RX_DATA,RSSI,Buf,Len);
      radioRxDequeuedPktRelease();
   }
}

