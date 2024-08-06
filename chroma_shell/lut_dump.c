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

struct {
   const char *CmdText;
   uint8_t CmdHex;
} g8176_cmd_lookup[] = {
// uc8154 like commands
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

uint8_t gScript[4096];

void LogEpdCmd(uint8_t Cmd)
{
   int i;
   
   for(i = 0; g8176_cmd_lookup[i].CmdText != NULL; i++) {
      if(Cmd == g8176_cmd_lookup[i].CmdHex) {
         printf("%s ",g8176_cmd_lookup[i].CmdText);
         break;
      }
   }

   if(g8176_cmd_lookup[i].CmdText == NULL) {
      printf("Unknown ");
   }
   printf("(0x%02x)",Cmd);
}

int DumpLutCmd(char *CmdLine)
{
   FILE *fp = NULL;
   uint8_t *p;
   int Ret = RESULT_FAIL; // assume the worse
   uint64_t NewOffset = 0;
   uint64_t Offset;
   long LutPageOffset = 0x2000;

   do {
      if(*CmdLine) {
         if((fp = fopen(CmdLine,"r")) == NULL) {
            LOG("fopen(\"%s\") failed - %s\n",CmdLine,strerror(errno));
            break;
         }
         if(fseek(fp,LutPageOffset,SEEK_SET)) {
            printf("fseek failed - %s\n",strerror(errno));
            break;
         }
         if(fread(gScript,sizeof(gScript),1,fp) != 1) {
            printf("fread failed - %s\n",strerror(errno));
            break;
         }
      }
// add support for reading from flash

      p = &gScript[0x51];
      printf("Starting LUT dump @ 0x51 (EEPROM adr 0x%lx)\n",LutPageOffset + 0x51);
      while(p < &gScript[sizeof(gScript)]) {
         uint8_t Opcode = *p++;
         int DataLen = (p[1] << 8) + p[0];
         p += 2;
#if 0
         printf("Opcode 0x%x, DataLen %d: ",Opcode,DataLen);
         if(DataLen > 0) {
            DumpHex(p,DataLen);
         }
         printf("\n");
#endif

         if(Opcode == 0 || DataLen == 0) {
            Offset = p - gScript;
            NewOffset = 0;
            switch(Offset) {
               case 0xa6:
                  NewOffset = 0x11e;
                  break;

               default:
                  if(*p != 0xff) {
                     NewOffset = Offset;
                  }
                  break;
            }

            if(NewOffset == 0) {
               printf("Stopped parsing at page offset 0x%lx\n",Offset);
               break;
            }
         }

         if(NewOffset != 0) {
            if(Offset != NewOffset ) {
               printf("skiping parsing from page offset 0x%lx to 0x%lx\n",
                      Offset,NewOffset);
               p = &gScript[NewOffset];
            }
            else {
               printf("parsing continuing at page offset 0x%lx\n",Offset);
            }
            NewOffset = 0;
            continue;
         }

         switch(Opcode) {
            case 0x4:
               printf("Wait for busy to go high\n");
            // Intentional fall though
            case 0x01:  // Send command and data to 
               LogEpdCmd(*p);
               if(DataLen > 1) {
                  printf("\n");
                  DumpHex(p+1,DataLen - 1);
               }
               printf("\n");
               break;

            default:
               printf("Opcode 0x%x ignored\n",Opcode);
               break;
         }
         p += DataLen;
      }
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }
   return Ret;
}


