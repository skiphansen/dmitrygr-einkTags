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
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"
#include "linenoise.h"
#include "cc1110-ext.h"
#include "chroma_shell.h"

typedef struct {
   const char *CmdText;
   uint8_t CmdHex;
} EpdCmdLut;

EpdCmdLut *gLutCmds;

const uint8_t gLutSignature[] = {0xce,0xfa,0xef,0xbe};   // "beefface"
void LutCompare(uint8_t *pData,int DataLen);


// 400 x 300 x 2
EpdCmdLut g8176_cmd_lookup[] = {
// uc8176 like commands
   {"PANEL_SETTING",0x00},
   {"POWER_SETTING",0x01},
   {"POWER_OFF",0x02},
   {"POWER_OFF_SEQUENCE",0x03},
   {"POWER_ON",0x04},
   {"POWER_ON_MEASURE",0x05},
   {"BOOSTER_SOFT_START",0x06},
   {"DEEP_SLEEP",0x07},
   {"DISPLAY_START_TRANSMISSION_DTM1",0x10},
   {"DATA_STOP",0x11},
   {"DISPLAY_REFRESH",0x12},
   {"DISPLAY_START_TRANSMISSION_DTM2",0x13},
   {"VCOM_LUT",   0x20},
   {"W2W_LUT",    0x21},
   {"B2W_LUT",    0x22},
   {"W2B_LUT",    0x23},
   {"B2B_LUT",    0x24},
   {"PLL_CONTROL",0x30},
   {"TEMPERATURE_CALIB",0x40},
   {"TEMPERATURE_SELECT",0x41},
   {"TEMPERATURE_WRITE",0x42},
   {"TEMPERATURE_READ",0x43},
   {"VCOM_INTERVAL",0x50},
   {"LOWER_POWER_DETECT",0x51},
   {"TCON_SETTING",0x60},
   {"RESOLUTION_SETTING",0x61},
   {"REVISION",0x70},
   {"STATUS",0x71},
   {"AUTO_MEASUREMENT_VCOM",0x80},
   {"READ_VCOM",0x81},
   {"VCOM_DC_SETTING",0x82},
   {"PARTIAL_WINDOW",0x90},
   {"PARTIAL_IN",0x91},
   {"PARTIAL_OUT",0x92},
   {"PROGRAM_MODE",0xA0},
   {"ACTIVE_PROGRAM",0xA1},
   {"READ_OTP",0xA2},
   {"CASCADE_SET",0xE0},
   {"POWER_SAVING",0xE3},
   {"FORCE_TEMPERATURE",0xE5},
   {NULL}     // end of table
};

// uc8159 like commands (640x480, 600x450,640,446,600,448)
EpdCmdLut g8159_cmd_lookup[] = {
   {"PANEL_SETTING",0x00},
   {"POWER_SETTING",0x01},
   {"POWER_OFF",0x02},
   {"POWER_OFF_SEQUENCE",0x03},
   {"POWER_ON",0x04},
   {"POWER_ON_MEASURE",0x05},
   {"BOOSTER_SOFT_START",0x06},
   {"DEEP_SLEEP",0x07},
   {"DISPLAY_START_TRANSMISSION_DTM1",0x10},
   {"DATA_STOP",0x11},
   {"DISPLAY_REFRESH",0x12},
   {"IPC",0x13},
   {"LUT_VCP,",0x20},
   {"LUT_B",0x21},
   {"LUT_W",0x22},
   {"LUT_G1",0x23},
   {"LUT_G2",0x24},
   {"LUT_R0",0x25},
   {"LUT_R1",0x26},
   {"LUT_R2",0x27},
   {"LUT_R3",0x28},
   {"LUT_XON",0x29},
   {"PLL_CONTROL",0x30},
   {"TEMPERATURE_CALIB",0x40},
   {"TEMPERATURE_SELECT",0x41},
   {"TEMPERATURE_WRITE",0x42},
   {"TEMPERATURE_READ",0x43},
   {"VCOM_INTERVAL",0x50},
   {"LOWER_POWER_DETECT",0x51},
   {"TCON_SETTING",0x60},
   {"RESOLUTION_SETTING",0x61},
   {"SPI_FLASH_CONTROL",0x65},
   {"REVISION",0x70},
   {"STATUS",0x71},
   {"AUTO_MEASUREMENT_VCOM",0x80},
   {"READ_VCOM",0x81},
   {"VCOM_DC_SETTING",0x82},
   {"PARTIAL_WINDOW",0x90},
   {"PARTIAL_IN",0x91},
   {"PARTIAL_OUT",0x92},
   {"PROGRAM_MODE",0xA0},
   {"ACTIVE_PROGRAM",0xA1},
   {"READ_OTP",0xA2},
   {"CASCADE_SET",0xE0},
   {"POWER_SAVING",0xE3},
   {"FORCE_TEMPERATURE",0xE5},
   {NULL}     // end of table
};

// uc8154 like commands 94x230, 94x252, 128x296, 200x300
EpdCmdLut g8154_cmd_lookup[] = {
   {"PANEL_SETTING",0x00},
   {"POWER_SETTING",0x01},
   {"POWER_OFF",0x02},
   {"POWER_OFF_SEQUENCE",0x03},
   {"POWER_ON",0x04},
   {"POWER_ON_MEASURE",0x05},
   {"BOOSTER_SOFT_START",0x06},
   {"DISPLAY_START_TRANSMISSION_DTM1",0x10},
   {"DATA_STOP",0x11},
   {"DISPLAY_REFRESH",0x12},
   {"DISPLAY_START_TRANSMISSION_DTM2",0x13},
   {"LUT_TC1,",0x20},
   {"LUT_W",0x21},
   {"LUT_B",0x22},
   {"LUT_G1",0x23},
   {"LUT_G2",0x24},
   {"LUT_C2",0x25},
   {"LUT_R0",0x26},
   {"LUT_R1",0x27},
   {"PLL_CONTROL",0x30},
   {"TEMPERATURE_CALIB",0x40},
   {"TEMPERATURE_SELECT",0x41},
   {"TEMPERATURE_WRITE",0x42},
   {"TEMPERATURE_READ",0x43},
   {"VCOM_INTERVAL",0x50},
   {"LOWER_POWER_DETECT",0x51},
   {"TCON_SETTING",0x60},
   {"RESOLUTION_SETTING",0x61},
   {"REVISION",0x70},
   {"STATUS",0x71},
   {"AUTO_MEASUREMENT_VCOM",0x80},
   {"READ_VCOM",0x81},
   {"VCOM_DC_SETTING",0x82},
   {NULL}     // end of table
};

EpdCmdLut g1675_cmd_lookup[] = {
   {"DRIVER CONTROL", 0x01},
   {"GATE VOLTAGE", 0x03},
   {"SOURCE VOLTAGE", 0x04},
   {"DISPLAY CONTROL", 0x07},
   {"NON OVERLAP", 0x0B},
   {"BOOSTER SOFT START", 0x0C},
   {"GATE SCAN START", 0x0F},
   {"DEEP SLEEP", 0x10},
   {"DATA MODE", 0x11},
   {"SW RESET", 0x12},
   {"TEMP SENSOR CTRL", 0x18},
   {"TEMP WRITE", 0x1A},
   {"TEMP READ", 0x1B},
   {"TEMP CONTROL", 0x1C},
   {"TEMP LOAD", 0x1D},
   {"MASTER ACTIVATE", 0x20},
   {"DISP CTRL1", 0x21},
   {"DISP CTRL2", 0x22},
   {"WRITE RAM", 0x24},
   {"WRITE ALTRAM", 0x26},
   {"READ RAM", 0x25},
   {"VCOM SENSE", 0x28},
   {"VCOM DURATION", 0x29},
   {"WRITE VCOM", 0x2C},
   {"READ OTP", 0x2D},
   {"WRITE LUT", 0x32},
   {"WRITE DUMMY", 0x3A},
   {"WRITE GATELINE", 0x3B},
   {"WRITE BORDER", 0x3C},
   {"SET RAMXPOS", 0x44},
   {"SET RAMYPOS", 0x45},
   {"SET RAMXCOUNT", 0x4E},
   {"SET RAMYCOUNT", 0x4F},
   {"NOP", 0xFF},
   {NULL}     // end of table
};

uint8_t gChroma42_8176_Luts[] = {
   43,
   0x21,  // W2W_LUT,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,

   45,
   0x20, // VCOM_LUT,
   0x00,0x18,0x00,0x00,0x00,0x01,0x00,0x84,
   0x84,0x00,0x00,0x01,0x00,0x0A,0x0A,0x00,
   0x00,0x1E,0x00,0x50,0x02,0x50,0x02,0x05,
   0x00,0x03,0x02,0x06,0x02,0x24,0x00,0x0A,
   0x05,0x32,0x05,0x06,0x00,0x05,0x06,0x32,
   0x03,0x08,0x00,0x00,

   43,
   0x23, // W2B_LUT,
   0x00,0x18,0x00,0x00,0x00,0x01,0x10,0x84,
   0x84,0x00,0x00,0x01,0x60,0x0A,0x0A,0x00,
   0x00,0x1E,0x48,0x50,0x02,0x50,0x02,0x05,
   0x80,0x03,0x02,0x06,0x02,0x24,0x00,0x0A,
   0x05,0x32,0x05,0x06,0x02,0x05,0x06,0x32,
   0x03,0x08,

   43,
   0x24, // B2B_LUT,
   0x00,0x18,0x00,0x00,0x00,0x01,0xA0,0x84,
   0x84,0x00,0x00,0x01,0x60,0x0A,0x0A,0x00,
   0x00,0x1E,0x48,0x50,0x02,0x50,0x02,0x05,
   0x04,0x03,0x02,0x06,0x02,0x24,0x00,0x0A,
   0x05,0x32,0x05,0x06,0x10,0x05,0x06,0x32,
   0x03,0x08,

   43,
   0x22, // B2W_LUT,
   0x80,0x18,0x00,0x00,0x00,0x01,0xA0,0x84,
   0x84,0x00,0x00,0x01,0x60,0x0A,0x0A,0x00,
   0x00,0x1E,0x48,0x50,0x02,0x50,0x02,0x05,
   0x84,0x03,0x02,0x06,0x02,0x24,0x8C,0x0A,
   0x05,0x32,0x05,0x06,0x8C,0x05,0x06,0x32,
   0x03,0x08,
   0
};

// uc8151 like commands
EpdCmdLut g8151_cmd_lookup[] = {
   {"PANEL_SETTING",0x00},
   {"POWER_SETTING",0x01},
   {"POWER_OFF",0x02},
   {"POWER_OFF_SEQUENCE",0x03},
   {"POWER_ON",0x04},
   {"POWER_ON_MEASURE",0x05},
   {"BOOSTER_SOFT_START",0x06},
   {"DEEP_SLEEP",0x07},
   {"DISPLAY_START_TRANSMISSION_DTM1",0x10},
   {"DATA_STOP",0x11},
   {"DISPLAY_REFRESH",0x12},
   {"DISPLAY_START_TRANSMISSION_DTM2",0x13},
   {"LUT_C,",0x20},
   {"LUT_WW",0x21},
   {"LUT_R",0x22},
   {"LUT_W",0x23},
   {"LUT_B",0x24},
   {"PLL_CONTROL",0x30},
   {"TEMPERATURE_CALIB",0x40},
   {"TEMPERATURE_SELECT",0x41},
   {"TEMPERATURE_WRITE",0x42},
   {"TEMPERATURE_READ",0x43},
   {"VCOM_INTERVAL",0x50},
   {"LOWER_POWER_DETECT",0x51},
   {"TCON_SETTING",0x60},
   {"RESOLUTION_SETTING",0x61},
   {"REVISION",0x70},
   {"STATUS",0x71},
   {"AUTO_MEASUREMENT_VCOM",0x80},
   {"READ_VCOM",0x81},
   {"VCOM_DC_SETTING",0x82},
   {"PARTIAL_IN",0x91},
   {"PARTIAL_OUT",0x92},
   {"ACTIVE_PROG",0xa1},
   {"READ_OTP",0xa2},
   {"CASCADE_SET",0xe0},
   {"FORCE_TEMP",0xe5},
   {NULL}     // end of table
};

uint8_t gScript[0x4000];

void LogEpdCmd(uint8_t Cmd)
{
   int i;
   
   for(i = 0; gLutCmds[i].CmdText != NULL; i++) {
      if(Cmd == gLutCmds[i].CmdHex) {
         printf("%s ",gLutCmds[i].CmdText);
         break;
      }
   }

   if(gLutCmds[i].CmdText == NULL) {
      printf("Unknown ");
   }
   printf("(0x%02x)",Cmd);
}

int DumpLutCmd(char *CmdLine)
{
   FILE *fp = NULL;
   int Ret = RESULT_FAIL; // assume the worse
   uint64_t NewOffset;
   uint64_t Offset = 0;
   long LutPageOffset = 0x2000;
   int LutNum = 1;
   uint64_t Skip1;
   size_t ScriptSize = 0x2000;
   uint8_t LutVerMajor;
   uint8_t LutVerMinor;

   do {
      if(GetSnCmd(CmdLine) != RESULT_OK) {
         break;
      }

      if(gSn == NULL || gChromaType == CHROMA_TYPE_UNKNOWN) {
         break;
      }

      if(*CmdLine) {
         if((fp = fopen(CmdLine,"r")) == NULL) {
            LOG("fopen(\"%s\") failed - %s\n",CmdLine,strerror(errno));
            break;
         }
         if(fseek(fp,LutPageOffset,SEEK_SET)) {
            printf("fseek failed - %s\n",strerror(errno));
            break;
         }
         if(fread(gScript,ScriptSize,1,fp) != 1) {
            printf("fread failed - %s\n",strerror(errno));
            break;
         }
      }
// TODO: add support for reading from flash

      if(memcmp(gScript,gLutSignature,sizeof(gLutSignature)) != 0) {
         printf("LUT signature not found at offset 0x%lx\n",LutPageOffset);
         DumpHex(gScript,sizeof(gLutSignature));
         break;
      }
      Ret = RESULT_OK; // assume good things for now
      Offset += 4;
      LutVerMajor = gScript[Offset];
      LutVerMinor = gScript[Offset+1];
      printf("LUT version %d.%d\n",LutVerMajor,LutVerMinor);
      Offset += 2;

      printf("LUT label '%-30s'\n",&gScript[Offset]);

      switch(gChromaType) {
         case CHROMA42_R:
         case CHROMA42_Y:
            gLutCmds = g8176_cmd_lookup;
            ScriptSize = 0x1000;
            Skip1 = 0x51;
            break;

         case CHROMA74_R:
         case CHROMA74_Y:
            gLutCmds = g8159_cmd_lookup;
            ScriptSize = 0x4000;
            Skip1 = 0x51;
            break;

         case CHROMA21_R:
         case CHROMA21_Y:
            gLutCmds = g8154_cmd_lookup;
            ScriptSize = 0x1000;
            Skip1 = 0xb5;
            Ret = RESULT_NO_SUPPORT;
            break;

         case CHROMA29C_R:
            gLutCmds = g8151_cmd_lookup;
            ScriptSize = 0x1000;
            Skip1 = 0x51;
            break;

         case CHROMA29_R:
         case CHROMA29_Y:
            gLutCmds = g8154_cmd_lookup;
            ScriptSize = 0x1000;
            if(LutVerMajor != 3) {
               Ret = RESULT_NO_SUPPORT;
               break;
            }
            switch(LutVerMinor) {
               case 2:
                  Skip1 = 0x54;
                  break;

               case 3:
                  Skip1 = 0x51;
                  break;

               default:
                  Ret = RESULT_NO_SUPPORT;
                  break;
            }
            break;

         case CHROMA29_CC1310_R:
         case CHROMA29_CC1310_Y:
            gLutCmds = g1675_cmd_lookup;
            ScriptSize = 0x1000;
            Skip1 = 0x50;
            break;

         case CHROMA21_CC1310_R:
            gLutCmds = g8176_cmd_lookup;
            ScriptSize = 0x1000;
            Skip1 = 0x50;
            break;

         default:
            LOG("Board type not defined\b");
            break;
      }

      if(Ret == RESULT_NO_SUPPORT) {
         printf("LUT version %d.%d is not supported, aborting.\n",
                LutVerMajor,LutVerMinor);
         break;
      }

      if(gLutCmds == NULL) {
         break;
      }

      Offset += 30;
// The first 69 (0x45) bytes @ offset 4 appear to be a header since
// it is read at boot (logic analyzer trace) so the first non-header
// byte is @ 0x04 + 0x45 = 0x049
      NewOffset = Skip1;
      while(Offset < sizeof(gScript)) {
         uint8_t Opcode = gScript[Offset];
         int DataLen = (gScript[Offset+2] << 8) + gScript[Offset + 1];

         if(NewOffset != 0) {
            if(Offset != NewOffset ) {
               uint64_t SkipLen = NewOffset - Offset;
               printf("skipping from offset 0x%lx to 0x%lx (%ld/0x%lx bytes)\n",
                      Offset + LutPageOffset,NewOffset + LutPageOffset,
                      SkipLen,SkipLen);
               DumpHex(&gScript[Offset],SkipLen);
               printf("\n");
               Offset = NewOffset;
            }
            else {
               printf("parsing continuing at offset 0x%lx\n",Offset + LutPageOffset);
            }
            NewOffset = 0;
            continue;
         }
#if 0
         if(Offset + DataLen > ScriptSize) {
            printf("Invalid DataLen %d, aborting.\n",DataLen);
            break;
         }
#endif

#if 0
         printf("0x%lx:\n",Offset + LutPageOffset);
         printf("Opcode 0x%x, DataLen %d: ",Opcode,DataLen);
         if(DataLen > 0) {
            DumpHex(&gScript[Offset+3],DataLen);
         }
         printf("\n");
#endif

         switch(Offset) {
            case 0xa3:
               if(gChromaType == CHROMA42_R || gChromaType == CHROMA42_Y) {
                  NewOffset = 0x11b;
                  NewOffset = 0x0e8;
               }
               break;

            case 0x16f:
               if(gChromaType == CHROMA74_R || gChromaType == CHROMA74_Y) {
                  NewOffset = 0x217;
               }
               break;

            case 0xa5:
               if(gChromaType == CHROMA29C_R) {
                  NewOffset = 0xf9;
               }
               break;
         }

         if(NewOffset != 0) {
            continue;
         }

         printf("\n");
         switch(Opcode) {
            case 0x00:
               printf("opcode 0: end of sequence @ 0x%lx\n",
                      LutPageOffset + Offset);
               break;

            case 0x01:  // Send command and data to 
               if(gChromaType == CHROMA42_R || gChromaType == CHROMA42_Y) {
                  if(gScript[Offset+3] == 0x01) {
                     printf("Start of LUT #%d @ 0x%lx\n\n",
                            LutNum++,LutPageOffset + Offset);
                  }
               }
               LogEpdCmd(gScript[Offset+3]);
               if(DataLen > 1) {
                  if(DataLen > 8) {
                     printf(" %d (0x%x) bytes of data:",
                            DataLen-1,DataLen-1);
                  }
                  if(DataLen + Offset > ScriptSize) {
                     printf("\nInvalid data Len %d, aborting.\n",DataLen);
                     Ret = RESULT_FAIL;
                     break;
                  }
                  else {
                     LutCompare(&gScript[Offset+3],DataLen);
                     printf("\n");
                     DumpHexSrc(&gScript[Offset+4],DataLen - 1);
                  }
               }
               Offset += DataLen;
               break;

            case 0x3:
               printf("opcode 3: wait for busy to go high\n");
               break;

            case 0x4:
               printf("opcode 4: write %d (0x%x) bytes of zeros to ",
                      DataLen-1,DataLen-1);
               LogEpdCmd(gScript[Offset+3]);
               printf("\n");
               Offset++;
               break;

            case 0x5:
               printf("opcode 5: wait for busy to go low\n");
               break;

            case 0x7:
               printf("opcode 7: Send data to display\n");
               break;

            case 0xff:
               printf("End of LUTs at offset 0x%lx\n",LutPageOffset + Offset);
               Offset = sizeof(gScript);
               break;

            default:
               if(DataLen > 1) {
                  printf("Opcode 0x%x at offset 0x%lx with %d bytes of data ignored:\n",
                         Opcode,Offset,DataLen - 1);
                  DumpHexAdr(&gScript[Offset],DataLen - 1,Offset+LutPageOffset);
               }
               else {
                  printf("Opcode 0x%x at offset 0x%lx ignored\n",Opcode,Offset);
               }
               break;
         }
         if(Ret != RESULT_OK) {
            break;
         }
         Offset += 3;
      }
      printf("page offset 0x%lx\n",LutPageOffset + Offset);
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   if(Ret != RESULT_OK) {
      int i = 0;
      printf("%s failed to parse\n",gSn != NULL ? gSn : CmdLine);
      for(i = ScriptSize - 1; i > 0; i--) {
         if(gScript[i] != 0xff) {
            break;
         }
      }
      printf("Dump:\n");
      DumpHexAdr(gScript,i+1,LutPageOffset);
      printf("\n");
      Ret = RESULT_FAIL;
   }
   else {
      printf("%s parsed successfully.\n\n",gSn);
   }

   return Ret;
}

void LutCompare(uint8_t *pData,int DataLen)
{
   uint8_t *p = gChroma42_8176_Luts;

   while(*p) {
      if(*p == DataLen) {
         if(memcmp(p+1,pData,DataLen) == 0) {
            printf(" (room temp LUT)");
         }
      }
      p += *p + 1;
   }
}

