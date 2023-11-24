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
#include <ctype.h>

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"
#include "linenoise.h"
#include "cc1110-ext.h"

#define DEFAULT_TO   100

#define EEPROM_WRITE_PAGE_SZ  256   // max write size & alignment
#define EEPROM_ERZ_SECTOR_SZ  4096  // erase size and alignment

int BoardTypeCmd(char *CmdLine);
int CwCmd(char *CmdLine);
int PingCmd(char *CmdLine);
int ResetCmd(char *CmdLine);
int EEPROM_ReadCmd(char *CmdLine);
int EEPROM_BackupCmd(char *CmdLine);
int DumpRfRegsCmd(char *CmdLine);
int SetRegCmd(char *CmdLine);
int DumpSettingsCmd(char *CmdLine);
int EEPROM_Internal(int Adr,FILE *fp,uint8_t *RdBuf,int Len);

// Eventual CC1101 API functions.  
// Function names based on https://github.com/LSatan/SmartRC-CC1101-Driver-Lib
int setSidle(void);
int SetTx(float mhz);

struct COMMAND_TABLE commandtable[] = {
   { "board_type",  "Display board type",NULL,0,BoardTypeCmd},
   { "cw",  "Turn on 915.0 Mhz carrier",NULL,0,CwCmd},
   { "dump_rf_regs", "Display settings of all RF registers",NULL,0,DumpRfRegsCmd},
   { "dump_settings", "Display settings in EEPROM",NULL,0,DumpSettingsCmd},
   { "eerd",  "Read data from EEPROM","eerd <address> <length>",0,EEPROM_ReadCmd},
   { "backup_eeprom",  "Write EEPROM data to a file","eerd <path>",0,EEPROM_BackupCmd},
   { "ping",  "Send a ping",NULL,0,PingCmd},
   { "reset", "reset device",NULL,0,ResetCmd},
   { "set_reg", "set chip register device",NULL,0,SetRegCmd},
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

typedef struct {
   uint16_t Adr;
   uint8_t  Value;
} RfSetting;

// Address Config = No address check 
// Base Frequency = 915.000000 
// CRC Enable = false 
// Carrier Frequency = 915.000000 
// Channel Number = 0 
// Channel Spacing = 199.951172 
// Data Rate = 1.19948 
// Deviation = 5.157471 
// Device Address = 0 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 255 
// Packet Length Mode = Reserved 
// Preamble Count = 4 
// RX Filter BW = 58.035714 
// Sync Word Qualifier Mode = No preamble/sync 
// TX Power = 10 
// Whitening = false 
// Rf settings for CC1110
RfSetting g915CW[] = {
   {0xdf04,0x22},  // PKTCTRL0: Packet Automation Control 
   {0xdf07,0x06},  // FSCTRL1: Frequency Synthesizer Control 
   {0xdf09,0x23},  // FREQ2: Frequency Control Word, High Byte 
   {0xdf0a,0x31},  // FREQ1: Frequency Control Word, Middle Byte 
   {0xdf0b,0x3B},  // FREQ0: Frequency Control Word, Low Byte 
   {0xdf0c,0xF5},  // MDMCFG4: Modem configuration 
   {0xdf0d,0x83},  // MDMCFG3: Modem Configuration 
   {0xdf0e,0x10},  // MDMCFG2: Modem Configuration 
   {0xdf11,0x15},  // DEVIATN: Modem Deviation Setting 
   {0xdf14,0x18},  // MCSM0: Main Radio Control State Machine Configuration 
   {0xdf15,0x17},  // FOCCFG: Frequency Offset Compensation Configuration 
   {0xdf1c,0xE9},  // FSCAL3: Frequency Synthesizer Calibration 
   {0xdf1d,0x2A},  // FSCAL2: Frequency Synthesizer Calibration 
   {0xdf1e,0x00},  // FSCAL1: Frequency Synthesizer Calibration 
   {0xdf1f,0x1F},  // FSCAL0: Frequency Synthesizer Calibration 
   {0xdf24,0x31},  // TEST1: Various Test Settings 
   {0xdf25,0x09},  // TEST0: Various Test Settings 
   {0xdf2e,0xC0},  // PA_TABLE0: PA Power Setting 0 
   {0}  // end of table
};

// Address Config = No address check 
// Base Frequency = 865.999634 
// CRC Enable = false 
// Carrier Frequency = 865.999634 
// Channel Number = 0 
// Channel Spacing = 199.951172 
// Data Rate = 1.19948 
// Deviation = 5.157471 
// Device Address = 0 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 255 
// Packet Length Mode = Reserved 
// Preamble Count = 4 
// RX Filter BW = 58.035714 
// Sync Word Qualifier Mode = No preamble/sync 
// TX Power = 10 
// Whitening = false 
// Rf settings for CC1110
RfSetting g866CW[] = {
   {0xdf04,0x22},  // PKTCTRL0: Packet Automation Control 
   {0xdf07,0x06},  // FSCTRL1: Frequency Synthesizer Control 
   {0xdf09,0x21},  // FREQ2: Frequency Control Word, High Byte 
   {0xdf0a,0x4E},  // FREQ1: Frequency Control Word, Middle Byte 
   {0xdf0b,0xC4},  // FREQ0: Frequency Control Word, Low Byte 
   {0xdf0c,0xF5},  // MDMCFG4: Modem configuration 
   {0xdf0d,0x83},  // MDMCFG3: Modem Configuration 
   {0xdf0e,0x10},  // MDMCFG2: Modem Configuration 
   {0xdf11,0x15},  // DEVIATN: Modem Deviation Setting 
   {0xdf14,0x18},  // MCSM0: Main Radio Control State Machine Configuration 
   {0xdf15,0x17},  // FOCCFG: Frequency Offset Compensation Configuration 
   {0xdf1c,0xE9},  // FSCAL3: Frequency Synthesizer Calibration 
   {0xdf1d,0x2A},  // FSCAL2: Frequency Synthesizer Calibration 
   {0xdf1e,0x00},  // FSCAL1: Frequency Synthesizer Calibration 
   {0xdf1f,0x1F},  // FSCAL0: Frequency Synthesizer Calibration 
   {0xdf24,0x31},  // TEST1: Various Test Settings 
   {0xdf25,0x09},  // TEST0: Various Test Settings 
   {0xdf2e,0xC2},  // PA_TABLE0: PA Power Setting 0 
   {0}  // end of table
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

int EEPROM_Internal(int Adr,FILE *fp,uint8_t *RdBuf,int Len)
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

         if(fp == NULL && RdBuf == NULL) {
         // Just dumping to the screen for a human consumer, 
         // ensure the second line dumped starts on an 16 byte boundary,
         // it's just easier to read that way
            if(Adr & 0xf) {
               int Adjusted = DUMP_BYTES_PER_LINE - (Adr & 0xf);
               if(Bytes2Read > Adjusted) {
                  Bytes2Read = Adjusted;
               }
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

         if((pMsg = Wait4Response(CMD_EEPROM_RD,2000)) == NULL) {
            break;
         }

         BytesRead += Bytes2Read;
         if(fp != NULL) {
         // Write data read to file
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
         else if(RdBuf != NULL) {
         // Copy data read to buffer
            memcpy(RdBuf,&pMsg->Msg[2],Bytes2Read);
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

   if(fp == NULL && RdBuf == NULL) {
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

      Ret = EEPROM_Internal(Adr,NULL,NULL,Len);
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
      Ret = EEPROM_Internal(0,fp,NULL,EEPROM_Len);
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

int SetRegCmd(char *CmdLine)
{
   return 0;
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
   int Ret = RESULT_BAD_ARG;  // assume the worse
   int i;
   uint8_t MacAdr[7];

   do {
   // Typical serial number: JM10339094B
   // To be valid the SN must:
   // 1. Be exactly 11 characters long and characters
   // 2. All characters must be a alphabetic character or a digit
   // 3. Characters 3 -> 10 must be digits
   // 
      if(*CmdLine == 0) {
         Ret = RESULT_USAGE;
         break;
      }
      *Skip2Space(CmdLine) = 0;
      if(strlen(CmdLine) != 11) {
         break;
      }
      for(i = 0; i < 11; i++) {
         if(i >= 2 && i < 10) {
            if(!isdigit(CmdLine[i])) {
               break;
            }
         }
         else {
            if(!isalnum(CmdLine[i])) {
                  break;
            }
         // force to uppercase
            CmdLine[i] = toupper(CmdLine[i]);
         }
      }
      if(i != 11) {
         break;
      }
      MacAdr[0] = CmdLine[0];
      MacAdr[1] = CmdLine[1];
      MacAdr[2] = (CmdLine[2] - '0') << 4 | (CmdLine[3] - '0');
      MacAdr[3] = (CmdLine[4] - '0') << 4 | (CmdLine[5] - '0');
      MacAdr[4] = (CmdLine[6] - '0') << 4 | (CmdLine[7] - '0');
      MacAdr[5] = (CmdLine[8] - '0') << 4 | (CmdLine[9] - '0');
      MacAdr[6] = CmdLine[10];
      LOG_RAW("MAC address: ");
      for(i = 0; i < sizeof(MacAdr); i++) {
         LOG_RAW("%s%02X",i > 0 ? ":" : "",MacAdr[i]);
      }
      LOG_RAW("\n");
      Ret = RESULT_OK;
   } while(false);

   if(Ret == RESULT_BAD_ARG) {
      LOG_RAW("Invalid serial number string \"%s\".\n",CmdLine);
      LOG_RAW("SN must be two characters followed by 8 digits followed by a chracter\n");
      LOG_RAW("For example \"JM10339094B\"\n");
      Ret = RESULT_OK;
   }

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

int CwCmd(char *CmdLine)
{
   uint8_t Cmd[512];
   int Ret = RESULT_OK;
   int MsgLen = 0;
   AsyncResp *pMsg;

   do {
      if(setSidle()) {
         break;
      }

      Cmd[MsgLen++] = CMD_SET_RF_REGS;
      for(int i = 0; g915CW[i].Adr != 0; i++) {
         Cmd[MsgLen++] = (uint8_t) (g915CW[i].Adr & 0xff);  // LSB only
         Cmd[MsgLen++] = g915CW[i].Value;
      }

      if((pMsg = SendCmd(Cmd,MsgLen,DEFAULT_TO)) != NULL) {
         free(pMsg);
      }
      if(SetTx(0.0)) {
         break;
      }
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

int SetRfState(uint8_t DesiredState)
{
   AsyncResp *pMsg;
   uint8_t Cmd[] = {CMD_SET_RF_MODE,DesiredState};
   int Ret = 0;   // Assume the best

   if((pMsg = SendCmd(Cmd,sizeof(Cmd),DEFAULT_TO)) != NULL) {
      free(pMsg);
   }
   else {
      Ret = 1;
   }

   return Ret;
}

// set state to idle
int setSidle()
{
   return SetRfState(RFST_SIDLE);
}

int SetTx(float mhz)
{
   int Ret = 0;   // Assume the best

   if(mhz == 0.0) {
   // just set the state
      Ret = SetRfState(RFST_STX);
   }
   else {
      ELOG("mhz 1= 0.0 not supported\n");
      Ret = 1;
   }

   return Ret;
}


int DumpSettingsCmd(char *CmdLine)
{
   static const uint8_t magicNum[4] = {0x56, 0x12, 0x09, 0x85};
   uint8_t Page;
   const char *Msg = NULL;
   int Adr;
   int EndAdr;
   int Type;
   int Len;
   bool end = false;
   uint8_t Data[4+256];
   int Err;

   for(Page = 0; Page < 10; Page++) {
      Adr = Page * EEPROM_ERZ_SECTOR_SZ;
      if((Err = EEPROM_Internal(Adr,NULL,Data,sizeof(magicNum))) != 0) {
         break;
      }

      if(memcmp(Data,magicNum,sizeof(magicNum)) == 0) {
         break;
      }
   }

   if(Page < 10) {
      LOG_RAW("Found setting's magic number in page %d @ 0x%x\n",Page,Adr);
      EndAdr = Adr + EEPROM_ERZ_SECTOR_SZ;
      Adr += 4;
      while(Err == 0 && Adr < EndAdr) {
         memset(Data,0xaa,sizeof(Data));
         if((Err = EEPROM_Internal(Adr,NULL,Data,2)) != 0) {
            break;
         }

      // first byte is type, (0xff for done), second is length
         Type = Data[0];
         Len = Data[1];

         if(Len < 1) {
            LOG_RAW("Len == %d, WTF?\n",Len);
            break;
         }

         switch(Type) {
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
               Msg = "01 MAC";
               break;

            case 0x2a:  // MAC
               Msg = "2A MAC";
               break;

            case 0xff:
               Msg = "end of settings";
               end = true;
               break;

            default:
               LOG_RAW("Unknown type 0x%x, %d bytes @ 0x%x\n",Type,Len,Adr);
               break;
         }

         if(Msg != NULL) {
            LOG_RAW("Found %d byte %s @ 0x%x\n",Len,Msg,Adr);
            Msg = NULL;
            if(end) {
               break;
            }
         }
         if(Type != 0 && Len > 2) {
            if((Err = EEPROM_Internal(Adr + 2,NULL,&Data[2],Len-2)) != 0) {
               break;
            }
            DumpHex(Data,Len);
         }
         Adr += Len;
      }
   }

   return RESULT_OK;
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



