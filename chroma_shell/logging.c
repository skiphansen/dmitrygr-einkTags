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
#include <ctype.h>
#include <time.h>
#include <stdint.h>
#include "logging.h"

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif

void DumpHex(void *AdrIn,int Len)
{
   unsigned char *Adr = (unsigned char *) AdrIn;
   int i = 0;
   int j;

   while(i < Len) {
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         LOG_RAW("%02x ",Adr[i+j]);
      }

      LOG_RAW(" ");
      for(j = 0; j < 16; j++) {
         if((i + j) == Len) {
            break;
         }
         if(isprint(Adr[i+j])) {
            LOG_RAW("%c",Adr[i+j]);
         }
         else {
            LOG_RAW(".");
         }
      }
      i += 16;
      LOG_RAW("\n");
   }
}

void DumpHexAdr(void *AdrIn,int Len,int AdrValue)
{
   unsigned char *Adr = (unsigned char *) AdrIn;
   int DumpLen;

   for(int i = 0; i < Len; i += 16) {
      LOG_RAW("%04X ",AdrValue);
      DumpLen = Len - i;
      if(DumpLen > 16) {
         DumpLen = 16;
      }
      DumpHex(Adr,DumpLen);
      AdrValue += 16;
      Adr += 16;
   }
}

void DumpHexSrc(void *AdrIn,int Len)
{
   unsigned char *Adr = (unsigned char *) AdrIn;
   int i = 0;
   int j;

   while(i < Len) {
      for(j = 0; j < 8; j++) {
         if((i + j) == Len) {
            break;
         }
         LOG_RAW("0x%02x,",Adr[i+j]);
      }
      i += 8;
      LOG_RAW("\n");
   }
}


