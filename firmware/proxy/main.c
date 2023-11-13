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
#include "SerialFraming.h"
#include "proxy_msgs.h"

#define LOG pr

void HandleMsg(void);

uint8_t gRxBuf[20];

int gRxMsgLen;

void main(void)
{
   clockingAndIntsInit();
   timerInit();
   boardInit();
   u1setUartMode();

   LOG("booted at 0x%04x\n", (uintptr_near_t)&main);
   SerialFrameIO_Init(gRxBuf,sizeof(gRxBuf));

   irqsOn();
   while(true) {
      uint8_t Byte;
      if(U1CSR & 0x04) {
         Byte = U1DBUF;
         gRxMsgLen = SerialFrameIO_ParseByte(Byte);
         if(gRxMsgLen > 0) {
         // Message avaliable
            HandleMsg();
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
   switch(gRxBuf[0]) {
      case CMD_PING:
      // reuse gRxBuf for the response 
         gRxBuf[0] = CMD_PING | CMD_RESP;
         gRxBuf[1] = CMD_ERR_NONE;
         SerialFrameIO_SendMsg(gRxBuf,2);
         break;

      default:
         pr("Unknown command 0x%x ignored\n",gRxBuf[0]);
         break;
   }
}
