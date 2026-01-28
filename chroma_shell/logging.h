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
#ifndef _LOGGING_H_
#define _LOGGING_H_

#ifdef __cplusplus
extern "C" { 
#endif
void _log(const char *fmt,...);
// The ALOG macro always prints
#ifndef ALOG
#define ALOG(format, ...) _log(format,## __VA_ARGS__)
#endif

#define PRINTF(format, ...) _log(format,## __VA_ARGS__)


void DumpHex(void *AdrIn,int Len);
void DumpHexAdr(void *AdrIn,int Len,int AdrValue);

#define DEBUG_LOGGING
#ifdef DEBUG_LOGGING
#define DUMP_HEX(x,y) DumpHex(x,y)
#define _LOG(format, ...) _log(format,## __VA_ARGS__)

#define ELOG(format, ...) _LOG("%s#%d: " format,__FUNCTION__,__LINE__,## __VA_ARGS__)
      
// The LOG macro only prints when the DEBUG define is set
// This macro adds the function name in from of the log message
#define LOG(format, ...) _LOG("%s: " format,__FUNCTION__,## __VA_ARGS__)

// The LOG_RAW macro only prints when the DEBUG define is set
// This macro is the same as LOG but without adding the function name
#define LOG_RAW(format, ...) _LOG(format,## __VA_ARGS__)

#else    // DEBUG

#define DumpHex(x,y)
#define DumpHexAdr(x,y,z)
#define ELOG(format, ...)
#define LOG(format, ...)
#define LOG_RAW(format, ...)
#define DUMP_HEX(x,y)
#endif   // DEBUG_LOGGING

void DumpHexSrc(void *AdrIn,int Len);

#ifdef __cplusplus
}
#endif
#endif   // _LOGGING_H_

