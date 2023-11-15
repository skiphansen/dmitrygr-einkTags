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
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"
#include "linenoise.h"

static uint32_t gEEPROM_Len;

int NopCmd(char *CmdLine);
int EEPROM_Read(char *CmdLine);
int ResetCmd(char *CmdLine);

struct COMMAND_TABLE commandtable[] = {
   { "eerd",  "Read data from EEPROM","eerd <address> <length>",0,EEPROM_Read},
   { "ping",  "Send a ping",NULL,0,NopCmd},
   { "reset", "reset device",NULL,0,ResetCmd},
   { "sn2mac",  "Convert a Chroma serial number string to MAC address",NULL,0,SN2MACCmd},
   { "?", NULL, NULL,CMD_FLAG_HIDE, HelpCmd},
   { "help",NULL, NULL,CMD_FLAG_HIDE, HelpCmd},
   { NULL}  // end of table
};

const char *Cmd2Str(uint8_t Cmd)
{
   static char ErrMsg[80];
   static const char *CmdStrings[] = {CMD_STRINGS};
   const char *Ret;

   if(Cmd == 0 || Cmd > CMD_LAST) {
      snprintf(ErrMsg,sizeof(ErrMsg),"Invalid Cmd %d (0x%x)",Cmd,Cmd);
      Ret = ErrMsg;
   }
   else {
      Ret = CmdStrings[Cmd-1];
   }

   return Ret;
}

const char *Rcode2Str(uint8_t Rcode)
{
   static char ErrMsg[80];
   static const char *ErrStrings[] = {CMD_ERR_STRINGS};
   const char *Ret;

   if(Rcode > CMD_ERR_LAST) {
      snprintf(ErrMsg,sizeof(ErrMsg),"Invalid rcode %d (0x%x)",Rcode,Rcode);
      Ret = ErrMsg;
   }
   else {
      Ret = ErrStrings[Rcode];
   }

   return Ret;
}

int EEPROM_Read(char *CmdLine)
{
   #define DUMP_BYTES_PER_LINE   16
   #define READ_CHUNK_LINES      4
   #define READ_CHUNK_LEN        (READ_CHUNK_LINES * 16)

   int Ret = RESULT_USAGE;
   int Adr;
   int Len;
   uint8_t Cmd[6];
   AsyncMsg *pMsg;
   int Bytes2Read;
   int Bytes2Dump;
   int DumpOffset;
   int BytesRead = 0;
   int MsgLen = 0;

   do {
      if(gEEPROM_Len == 0) {
         Cmd[0] = CMD_EEPROM_LEN;
         if(SendAsyncMsg(&Cmd[0],1) != 0) {
            break;
         }

         if((pMsg = Wait4Response(CMD_EEPROM_LEN,100)) == NULL) {
            break;
         }
         memcpy(&gEEPROM_Len,&pMsg->Msg[2],sizeof(gEEPROM_Len));
         PRINTF("EEPROM len %dK (%d) bytes\n",gEEPROM_Len / 1024,gEEPROM_Len);
         free(pMsg);
      }

      if(sscanf(CmdLine,"%x %d",&Adr,&Len) != 2) {
         break;
      }
      if(Adr < 0 || Adr > gEEPROM_Len-1) {
         LOG_RAW("Invalid address (0x%x > 0x%x)\n",Adr,gEEPROM_Len-1);
         break;
      }
      if(Len < 0 || (Len + Adr) > gEEPROM_Len) {
         PRINTF("Invalid length %d\n",Len);
         break;
      }

      while(BytesRead < Len) {
         Bytes2Read = Len;
      // ensure the second line dumped starts on an 16 byte boundary,
      // it's just easier to read that way
         if(Adr & 0xf) {
            Bytes2Read = DUMP_BYTES_PER_LINE - (Adr & 0xf);
         }
         if(Bytes2Read > READ_CHUNK_LEN) {
            Bytes2Read = READ_CHUNK_LEN;
         }
         MsgLen = 0;

         Cmd[MsgLen++] = CMD_EEPROM_RD;
         Cmd[MsgLen++] = (uint8_t) (Bytes2Read & 0xff);
         Cmd[MsgLen++] = (uint8_t) ((Bytes2Read >> 8)& 0xff);
         Cmd[MsgLen++] = (uint8_t) (Adr & 0xff);
         Cmd[MsgLen++] = (uint8_t) ((Adr >> 8) & 0xff);
         Cmd[MsgLen++] = (uint8_t) ((Adr >> 16) & 0xff);
         if(SendAsyncMsg(Cmd,MsgLen) != 0) {
            break;
         }

         if((pMsg = Wait4Response(CMD_EEPROM_RD,100)) == NULL) {
            break;
         }
         BytesRead += Bytes2Read;
         DumpOffset = 2;
         while(Bytes2Read > 0) {
            LOG_RAW("%06x ",Adr);
            Bytes2Dump = Bytes2Read;
            if(Bytes2Dump > DUMP_BYTES_PER_LINE) {
               Bytes2Dump = DUMP_BYTES_PER_LINE;
            }
            DumpHex(&pMsg->Msg[DumpOffset],Bytes2Dump);
            Adr += Bytes2Dump;
            DumpOffset += Bytes2Dump;
            Bytes2Read -= Bytes2Dump;
         }
         free(pMsg);
      }
      Ret = RESULT_OK;
   } while(false);

   return Ret;
}

int NopCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd = CMD_PING;

   SendAsyncMsg(&Cmd,1);

   return Ret;
}

int ResetCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd = CMD_RESET;

   SendAsyncMsg(&Cmd,1);

   return Ret;
}

int SN2MACCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best

   return Ret;
}

void HandleResp(uint8_t *Msg,int MsgLen)
{
   uint8_t Cmd = Msg[0] & ~CMD_RESP;

   if(Msg[1] != 0) {
      ELOG("Command %s returned %s\n",Cmd2Str(Cmd),Rcode2Str(Msg[1]));
   }
   else {
      uint16_t *pU16 = (uint16_t *) &Msg[2];
      char *pChar = (char *) &Msg[2];

      switch(Cmd) {
         case CMD_PING:
            PrintResponse("Ping response received\n");
            break;

         case CMD_EEPROM_RD:
            break;

         case CMD_EEPROM_LEN:
            break;

         case CMD_COMM_BUF_LEN:
            PrintResponse("Communications buffer len %d bytes\n",*pU16);
            break;

         case CMD_BOARD_TYPE:
            PrintResponse("Board type %s\n",pChar);
            break;

         default:
            if(Cmd > CMD_LAST) {
               PrintResponse("Unknown response received 0x%x\n",Msg[0]);
            }
            break;
      }
   }
}

void Usage()
{
   PRINTF("Usage: fw_update -D <path> [options]\n");
   PRINTF("  options:\n");
   PRINTF("\t-c<command to run>\tRun specified command and exit\n");
   PRINTF("\t-d\tDebug mode (run in foreground)\n");
   PRINTF("\t-D<path>\tSet path to async device (for example /etc/ttyUSB0)\n");
   PRINTF("\t-i\tInteractive (command line mode)\n");
   PRINTF("\t-q\t\tquient\n");
   PRINTF("\t-v?\t\tList available verbose display levels\n");
   PRINTF("\t-v<bitmap>\tSet desired display levels (Hex bit map)\n");
}



