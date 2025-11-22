/*
 *  Copyright (C) 2024  Skip Hansen
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

#ifndef _CHROMA_SHELL_H
#define _CHROMA_SHELL_H

typedef enum {
   CHROMA_TYPE_UNKNOWN,
   CHROMA16_R,
   CHROMA16_Y,
   CHROMA21_R,
   CHROMA21_Y,
   CHROMA29_R,
   CHROMA29_Y,
   CHROMA29C_R,
   CHROMA29_CC1310_R,
   CHROMA29_CC1310_Y,
   CHROMA42_R,
   CHROMA42_Y,
   CHROMA60_R,
   CHROMA60_Y,
   CHROMA74_R,
   CHROMA74_Y,
   CHROMA74_CC1311_Y,
   CHROMA21_CC1310_R,
   CHROMA74_CC1310_R,
   CHROMA74_CC1310_Y,
} ChromaType;

typedef struct {
   size_t TblSize;
   uint8_t *pTbl;
} InitTbl;

extern InitTbl Chroma42_C8154InitTbl[];
extern char *gSn;
extern ChromaType gChromaType;

int GetSnCmd(char *CmdLine);


#endif   // _CHROMA_SHELL_H
