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
#include <errno.h>

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"
#include "linenoise.h"


int BoardTypeCmd(char *CmdLine);
int PingCmd(char *CmdLine);
int ResetCmd(char *CmdLine);
int EEPROM_ReadCmd(char *CmdLine);
int EEPROM_BackupCmd(char *CmdLine);
int DumpRfRegsCmd(char *CmdLine);

struct COMMAND_TABLE commandtable[] = {
   { "board_type",  "Display board type",NULL,0,BoardTypeCmd},
   { "dump_rf_regs", "Display settings of all RF registers",NULL,0,DumpRfRegsCmd},
   { "eerd",  "Read data from EEPROM","eerd <address> <length>",0,EEPROM_ReadCmd},
   { "backup_eeprom",  "Write EEPROM data to a file","eerd <path>",0,EEPROM_BackupCmd},
   { "ping",  "Send a ping",NULL,0,PingCmd},
   { "reset", "reset device",NULL,0,ResetCmd},
   { "sn2mac",  "Convert a Chroma serial number string to MAC address",NULL,0,SN2MACCmd},
   { "?", NULL, NULL,CMD_FLAG_HIDE, HelpCmd},
   { "help",NULL, NULL,CMD_FLAG_HIDE, HelpCmd},
   { NULL}  // end of table
};

const struct {
   const char *Name;
   uint16_t    Adr;
} CC111xRegs[] = {
   {"SYNC1",0xdf00},
   {"SYNC0",0xdf01},
   {"PKTLEN",0xdf02},
   {"PKTCTRL1",0xdf03},
   {"PKTCTRL0",0xdf04},
   {"ADDR",0xdf05},
   {"CHANNR",0xdf06},
   {"FSCTRL1",0xdf07},
   {"FSCTRL0",0xdf08},
   {"FREQ2",0xdf09},
   {"FREQ1",0xdf0a},
   {"FREQ0",0xdf0b},
   {"MDMCFG4",0xdf0c},
   {"MDMCFG3",0xdf0d},
   {"MDMCFG2",0xdf0e},
   {"MDMCFG1",0xdf0f},
   {"MDMCFG0",0xdf10},
   {"DEVIATN",0xdf11},
   {"MCSM2",0xdf12},
   {"MCSM1",0xdf13},
   {"MCSM0",0xdf14},
   {"FOCCFG",0xdf15},
   {"BSCFG",0xdf16},
   {"AGCCTRL2",0xdf17},
   {"AGCCTRL1",0xdf18},
   {"AGCCTRL0",0xdf19},
   {"FREND1",0xdf1a},
   {"FREND0",0xdf1b},
   {"FSCAL3",0xdf1c},
   {"FSCAL2",0xdf1d},
   {"FSCAL1",0xdf1e},
   {"FSCAL0",0xdf1f},
   {"TEST2",0xdf23},
   {"TEST1",0xdf24},
   {"TEST0",0xdf25},
   {"PA_TABLE7",0xdf27},
   {"PA_TABLE6",0xdf28},
   {"PA_TABLE5",0xdf29},
   {"PA_TABLE4",0xdf2a},
   {"PA_TABLE3",0xdf2b},
   {"PA_TABLE2",0xdf2c},
   {"PA_TABLE1",0xdf2d},
   {"PA_TABLE0",0xdf2e},
   {"IOCFG2",0xdf2f},
   {"IOCFG1",0xdf30},
   {"IOCFG0",0xdf31},
   {"PARTNUM",0xdf36},
   {"VERSION",0xdf37},
   {"FREQEST",0xdf38},
   {"LQI",0xdf39},
   {"RSSI",0xdf3a},
   {"MARCSTATE",0xdf3b},
   {"PKTSTATUS",0xdf3c},
   {"VCO_VC_DAC",0xdf3d}
};

int EEPROM_Internal(int Adr,int Len,FILE *fp);

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

int GetEEPROM_Len(void)
{
   static int EEPROM_Len;
   AsyncMsg *pMsg;
   uint8_t Cmd[2];

   if(EEPROM_Len == 0) do {
      Cmd[0] = CMD_EEPROM_LEN;
      if(SendAsyncMsg(&Cmd[0],1) != 0) {
         break;
      }

      if((pMsg = Wait4Response(CMD_EEPROM_LEN,100)) == NULL) {
         break;
      }
      memcpy(&EEPROM_Len,&pMsg->Msg[2],sizeof(EEPROM_Len));
      free(pMsg);
   } while(false);

   return EEPROM_Len;
}

int EEPROM_Internal(int Adr,int Len,FILE *fp)
{
   #define DUMP_BYTES_PER_LINE   16
   #define READ_CHUNK_LINES      4
   #define READ_CHUNK_LEN        (READ_CHUNK_LINES * 16)

   int Ret = RESULT_USAGE;
   uint8_t Cmd[6];
   AsyncMsg *pMsg;
   int Bytes2Read;
   int Bytes2Dump;
   int DumpOffset;
   int BytesRead = 0;
   int MsgLen = 0;
   int LastProgress = -1;
   int Progress = 0;

   do {
      while(BytesRead < Len) {
         Bytes2Read = (Len - BytesRead);
      // ensure the second line dumped starts on an 16 byte boundary,
      // it's just easier to read that way
         if(Adr & 0xf) {
            int Adjusted = DUMP_BYTES_PER_LINE - (Adr & 0xf);
            if(Bytes2Read > Adjusted) {
               Bytes2Read = Adjusted;
            }
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
         if(fp != NULL) {
         // Saving EEPROM
            Progress = (100 * BytesRead) / Len;
            if(LastProgress != Progress) {
               LastProgress = Progress;
               printf("\r%d%% complete",LastProgress);
               fflush(stdout);
            }
            if(fwrite(&pMsg->Msg[2],Bytes2Read,1,fp) != 1) {
               printf("fwrite failed\n");
               break;
            }
            Adr += Bytes2Read;
         }
         else {
         // Dumping EEPROM
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
         }
         free(pMsg);
      }
      Ret = RESULT_OK;
   } while(false);

   if(fp != NULL) {
      printf("\n");
   }

   return Ret;
}

int EEPROM_ReadCmd(char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int Adr;
   int Len;
   int EEPROM_Len = GetEEPROM_Len();

   do {
      if(sscanf(CmdLine,"%x %d",&Adr,&Len) != 2) {
         break;
      }
      if(Adr < 0 || Adr > EEPROM_Len-1) {
         LOG_RAW("Invalid address (0x%x > 0x%x)\n",Adr,EEPROM_Len-1);
         break;
      }
      if(Len < 0 || (Len + Adr) > EEPROM_Len) {
         PRINTF("Invalid length %d\n",Len);
         break;
      }

      Ret = EEPROM_Internal(Adr,Len,NULL);
   } while(false);

   return Ret;
}

int EEPROM_BackupCmd(char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int EEPROM_Len = GetEEPROM_Len();
   FILE *fp = NULL;

   do {
      printf("EEPROM len %dK (%d) bytes\n",EEPROM_Len / 1024,EEPROM_Len);
      if((fp = fopen(CmdLine,"w")) == NULL) {
         LOG("fopen(\"%s\") failed - %s\n",strerror(errno));
         Ret = RESULT_FAIL;
         break;
      }
      Ret = EEPROM_Internal(0,EEPROM_Len,fp);
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

int DumpRfRegsCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   AsyncMsg *pMsg;
   uint8_t Cmd[2];
   int j = 0;

   do {
      Cmd[0] = CMD_GET_RF_REGS;
      if(SendAsyncMsg(&Cmd[0],1) != 0) {
         break;
      }

      if((pMsg = Wait4Response(Cmd[0],100)) == NULL) {
         break;
      }
      for(int i = 2; i < pMsg->MsgLen; i++) {
         if((CC111xRegs[j].Adr & 0xff) != i - 2) {
            continue;
         }
         printf("%10s: 0x%02x\n",CC111xRegs[j++].Name,pMsg->Msg[i]);
      }
      DumpHex(&pMsg->Msg[2],pMsg->MsgLen);
      free(pMsg);
   } while(false);

   return Ret;
}


int PingCmd(char *CmdLine)
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

int BoardTypeCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd[2] = {CMD_BOARD_TYPE};
   AsyncMsg *pMsg;

   do {
      if(SendAsyncMsg(&Cmd[0],1) != 0) {
         break;
      }

      if((pMsg = Wait4Response(Cmd[0],100)) == NULL) {
         break;
      }
      printf("Board type %s\n",&pMsg->Msg[2]);
      free(pMsg);
   } while(false);

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
            break;

         default:
            if(Cmd > CMD_LAST) {
               PrintResponse("Unknown response received 0x%x\n",Msg[0]);
            }
            break;
      }
   }
}

#if 0
void DumpSetting()
{
   static const uint8_t magicNum[4] = {0x56, 0x12, 0x09, 0x85};
   uint8_t tmpBuf[4];
   uint8_t pg, gotLen = 0;
   const char *Msg = NULL;
   uint16_t addr;
   uint16_t ofst;
   bool end = false;

   for(pg = 0; pg < 10; pg++) {
      addr = pg EEPROM_ERZ_SECTOR_SZ;
      eepromRead(addr, tmpBuf, 4);

      if(xMemEqual(tmpBuf, magicNum, 4)) {
         ofst = 4;
         pr("Found setting's magic number in page %d @ 0x%x\n",pg,addr);
         while(ofst < EEPROM_ERZ_SECTOR_SZ) {
            eepromRead(addr + ofst, tmpBuf, 2);// first byte is type, (0xff for done), second is length

            switch(tmpBuf[0]) {
               case 0x0:
                  break;

               case 0x9:  // ADC intercept
                  Msg = "ADC intercept";
                  break;

               case 0x12:  // ADC slope
                  Msg = "ADC slope";
                  break;

               case 0x23:  // VCOM
                  Msg = "VCOM";
                  break;

               case 0x01:  // MAC
               case 0x2a:  // MAC
                  Msg = "MAC";
                  break;

               case 0xff:
                  Msg = "end of settings";
                  end = true;
                  break;

               default:
                  pr("Unknown type 0x%x, %d bytes @ 0x%x\n",
                     tmpBuf[0],tmpBuf[1],addr + ofst);
                  break;
            }

            if(Msg != NULL) {
               pr("Found %d byte %s @ 0x%x\n",tmpBuf[1],Msg,addr + ofst);
               Msg = NULL;
               if(end) {
                  break;
               }
            }
            ofst += tmpBuf[1];
            if(tmpBuf[1] == 0) {
               break;
            }
         }
      }
   }
}
#endif

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



