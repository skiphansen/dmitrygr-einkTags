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
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"


struct COMMAND_TABLE commandtable[] = {
   { "ping",  "Send a ping",NULL,0,NopCmd},
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

int NopCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd = CMD_PING;

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
   switch(Cmd) {
      case CMD_PING:
         PrintResponse("Ping response received\n");
         break;

      default:
         if(Cmd > CMD_LAST) {
            PrintResponse("Unknown response received 0x%x\n",Msg[0]);
         }
         break;
   }
}

void Usage()
{
   printf("Usage: fw_update -D <path> [options]\n");
   printf("  options:\n");
   printf("\t-c<command to run>\tRun specified command and exit\n");
   printf("\t-d\tDebug mode (run in foreground)\n");
   printf("\t-D<path>\tSet path to async device (for example /etc/ttyUSB0)\n");
   printf("\t-i\tInteractive (command line mode)\n");
   printf("\t-q\t\tquient\n");
   printf("\t-v?\t\tList available verbose display levels\n");
   printf("\t-v<bitmap>\tSet desired display levels (Hex bit map)\n");
}



