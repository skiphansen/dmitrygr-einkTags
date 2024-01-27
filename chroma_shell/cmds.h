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
#ifndef _CMDS_H_
#define _CMDS_H_

#define DEFAULT_BAUDRATE   1000000
#define DEFAULT_DEVICE     "/dev/ttyUSB0"
#define HISTORY_FILENAME   "chroma_history"

struct COMMAND_TABLE {
   char *CmdString;
   char *HelpString;
   char *Usage;
   const int Flags;
   int (*CmdHandler)(char *CmdLine);
};

extern struct COMMAND_TABLE commandtable[];

#define CMD_FLAG_HIDE      1
#define CMD_FLAG_EXACT     2

// Test command, display results
#define CMD_FLAG_TEST      4

// Return values from commands
#define RESULT_OK          0
#define RESULT_FAIL        1
#define RESULT_USAGE       2
#define RESULT_NO_SUPPORT  3
#define RESULT_BAD_ARG     4
#define RESULT_TIMEOUT     5
#define RESULT_BAD_LEN     6


void Usage(void);
int NopCmd(char *CmdLine);
void HandleResp(uint8_t *Msg,int MsgLen);
const char *Rcode2Str(uint8_t Rcode);
const char *Cmd2Str(uint8_t Cmd);
int HelpCmd(char *CmdLine);
void HandleCmd(uint8_t *Msg,int MsgLen);

#endif   // _CMDS_H_

