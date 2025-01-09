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

typedef struct {
   size_t TblSize;
   uint8_t *pTbl;
} InitTbl;

extern InitTbl Chroma42_C8154InitTbl[];

#endif   // _CHROMA_SHELL_H
