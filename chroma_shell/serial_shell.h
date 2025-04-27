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
#ifndef _SHELL_H
#define _SHELL_H

extern struct linenoiseState gLs;
extern char *gDevicePath;


#define CMD_RESP     0x80  

// Generic wrapper for both commands and responses
typedef struct AsyncMsg_TAG {
   struct AsyncMsg_TAG *Link; // NB: must be first
   int MsgLen;
   uint8_t  Msg[];   // Actually variable length
} AsyncMsg;

typedef struct {
   AsyncMsg *Link;   // NB: must be first
   int MsgLen;
   uint8_t  RespCmd; // Command | CMD_RESP
   uint8_t  Err;     // 0 or error code
   uint8_t  Msg[];   // variable length data
} AsyncResp;

typedef struct {
   AsyncMsg *Link;   // NB: must be first
   int MsgLen;
   uint8_t  RespCmd; // Command | CMD_RESP
   uint8_t  Msg[];   // variable length data
} AsyncCmd;

extern AsyncMsg *gMsgQueueHead;
extern AsyncMsg *gMsgQueueTail;

int SendAsyncMsg(uint8_t *Msg,int MsgLen);
void PrintResponse(const char *fmt, ...);
AsyncMsg *Wait4Response(uint8_t Cmd,int Timeout);
AsyncResp *SendCmd(uint8_t *Msg,int MsgLen,int Timeout);
char *Skip2Space(char *In);
void Sleep(int Milliseconds);
char *NextToken(char *In);
int ConvertValue(char **Arg,uint32_t *Value);

#endif // _SHELL_H
