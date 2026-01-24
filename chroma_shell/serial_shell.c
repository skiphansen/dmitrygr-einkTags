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
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <stdarg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <linux/serial.h>
#include "logging.h"
#include "linenoise.h"
#include "cmds.h"
#include "serial_shell.h"
#include "CobsFraming.h"

//#define LOOP_BACK_TEST  1

#define PROMPT    "chroma>"
#define MAX_STR   255
#define OPTION_STRING      "b:c:dD:qv:"

#define FWVER_MAX_LEN         8

AsyncMsg *gMsgQueueHead;
AsyncMsg *gMsgQueueTail;
int gSerialFd = -1;
int gSerialBaudrate = DEFAULT_BAUDRATE;

static struct COMMAND_TABLE *pCurrentCmd;
bool gDebug;
struct timeval gTimeNow;
int gQuiet;
struct linenoiseState gLs;
// return values for GetMessage
typedef enum {
   NO_MSG = 0,
   TIMEOUT,
   STDIN_AVAIL,
   MSG_READY
} GetMessageStatus;

char *gDevicePath = NULL;

typedef int (*ConsoleCB)(const char *Line);
typedef int (*SerialDataCB)();

int WaitEvent(int Timeout,ConsoleCB,SerialDataCB);
bool OpenSerialPort(const char *Device,int Baudrate);
bool SendSerialData(uint8_t *Buf,int Len);
int RecvSerialData(uint8_t *Buf,int BufLen);
static void PrintHeader(void);
int MainLoop(void);
static struct COMMAND_TABLE *FindCmd(char *CmdLine,int bSilent);
void ParseCmd(char *CmdLine);
int MatchCmd(char *CmdLine, char *Command);
char *SkipSpaces(char *In);
void HandleCmd(uint8_t *Msg,int MsgLen);
int ParseSerialData(uint8_t *Buf,int Len);
void HandleResp(uint8_t *Msg,int MsgLen);
int ParseData(uint8_t Data);
void SendByte(uint8_t SendByte);
void Usage(void);
int ProcessSerialData(void);
void PrintTime(bool bCr);
char *TimeStamp2String(uint32_t TAISecs);
void DisplayTime(void);
void UpdateTimeNow(void);
void SetTimeout(int Timeout,struct timeval *pTmr);
bool IsTimedOut(struct timeval *pTmr);
struct COMMAND_TABLE *GetArgument(char *CmdLine,struct COMMAND_TABLE *pCmd);
void PrintArgList(struct COMMAND_TABLE *pCmd,bool bVerbose);
const char *Cmd2Str(uint8_t Cmd);
const char *Rcode2Str(uint8_t Rcode);
void TabCompletion(const char *buf, linenoiseCompletions *lc);
void RunOneCommand(char *Command);
char *GetHistoryPath();

#define VERBOSE_V1               0x01
#define VERBOSE_DUMP_RAW_TX      0x08
#define VERBOSE_DUMP_RAW_RX      0x10
#define VERBOSE_SHOW_DEBUG_OUT   0x20   
#define VERBOSE_TIMESTAMPS       0x40
#define VERBOSE_DUMP_RAW_MSGS    0x80

struct {
   int   Mask;
   char *Desc;
} DebugMasks[] = {
   {VERBOSE_V1,"Generally verbose"},
   {VERBOSE_DUMP_RAW_TX,"Dump RAW async data sent"},
   {VERBOSE_DUMP_RAW_RX,"Dump RAW async data received"},
   {VERBOSE_SHOW_DEBUG_OUT,"Display debug output from device"},
   {VERBOSE_TIMESTAMPS,"Time stamp RAW I/O"},
   {VERBOSE_DUMP_RAW_MSGS,"Display RAW messages received"},
   {0},  // end of table
};

typedef struct {
   bool PromptDisplayed;
   bool InCommand;
   bool LineNoiseHidden;
} Flags;
Flags gFlags;

typedef struct {
   unsigned int Verbose;
} Globals;
Globals g;

void SendByte(uint8_t SendByte);

#define ASYNC_MSG_BUF_LEN  128
uint8_t gTxBuf[ASYNC_MSG_BUF_LEN];
int gTxMsgLen;
int gMaxMsgLen;
uint8_t RxBuf[ASYNC_MSG_BUF_LEN];

int main(int argc, char* argv[])
{
   int Ret = 0;
   int Option;
   int bExit = false;
   char *ImmediateCommand = NULL;


// Set default device on Linux
   gDevicePath = strdup(DEFAULT_DEVICE);  // Set default path

   while((Option = getopt(argc,argv,OPTION_STRING)) != -1) {
      switch(Option) {
         case 'b':
            if(sscanf(optarg,"%d",&gSerialBaudrate) != 1) {
               Ret = EINVAL;
            }
            break;

         case 'c':
            if(ImmediateCommand != NULL) {
               free(ImmediateCommand);
            }
            ImmediateCommand = strdup(optarg);
            break;

         case 'd':
            gDebug = true;
            break;

         case 'D':
            if(gDevicePath != NULL) {
               free(gDevicePath);
            }
            gDevicePath = strdup(optarg);
            break;

         case 'q':
            gQuiet = true;
            break;

         case 'v':
            if(*optarg == '?') {
               int i;
               printf("Verbose logging levels available:\n");
               for(i = 0; DebugMasks[i].Mask != 0; i++) {
                  printf("%08x - %s\n",DebugMasks[i].Mask,DebugMasks[i].Desc);
               }
               bExit = true;
            }
            else if(sscanf(optarg,"%x",&g.Verbose) != 1) {
               Ret = EINVAL;
            }
            break;

         default:
            Ret = EINVAL;
            break;
      }
   }

   if(Ret != 0 || gDevicePath == NULL ) {
      Usage();
      bExit = true;
   }

   if(Ret == 0 && !bExit) do {
      if(gDevicePath == NULL) {
         printf("Error: device path not specified (-D <device path>)\n");
         break;
      }
      if(!OpenSerialPort(gDevicePath,gSerialBaudrate)) {
         break;
      }
      gMaxMsgLen = SerialFrameIO_Init(RxBuf,sizeof(RxBuf)-1);
      linenoiseHistoryLoad(GetHistoryPath());
      linenoiseSetCompletionCallback(TabCompletion);
      if(ImmediateCommand == NULL) {
         MainLoop();
      }
      else {
         RunOneCommand(ImmediateCommand);
      }
   } while(false);

   if(gDevicePath != NULL) {
      free(gDevicePath);
   }

   if(ImmediateCommand != NULL) {
      free(ImmediateCommand);
   }

   return Ret;
}

static void PrintHeader()
{
   LOG_RAW("chroma shell v0.01, compiled " __DATE__" " __TIME__ "\n");
}


// Returns:
//    < 0 - error
//    = 0 - timeout
//    > 0 - Did something
int ConCB(const char *CmdLine)
{
   int Ret = 0;
   char *cp;

   do {
      linenoiseEditStop(&gLs);
      if(CmdLine == NULL || CmdLine[0] == 0) {
      // empty line or Ctrl-C save history and exit
         linenoiseHistorySave(GetHistoryPath());
         Ret = -1;
         break;
      }
      linenoiseHistoryAdd(CmdLine);
      if((cp = strchr(CmdLine,'\n')) != NULL) {
         *cp = 0; // remove newline from CmdLine
      }
   // process command
      gFlags.PromptDisplayed = false;
      gFlags.InCommand = true;
      ParseCmd((char *)CmdLine);
      gFlags.InCommand = false;
   } while(false);

   return Ret;
}


// forever loop
int MainLoop()
{
   int Ret;
   AsyncMsg *pMsg;
   uint8_t Cmd;
   char CmdLineBuf[1024];

   PrintHeader();
   fflush(stdout);
   for( ; ; ) {
      if(!gFlags.PromptDisplayed) {
         gFlags.PromptDisplayed = true;
         linenoiseEditStart(&gLs,-1,-1,CmdLineBuf,sizeof(CmdLineBuf),PROMPT);
         fflush(stdout);
      }

      if(gFlags.LineNoiseHidden) {
         gFlags.LineNoiseHidden = false;
         linenoiseShow(&gLs);
      }

      if((Ret = WaitEvent(100,ConCB,ProcessSerialData)) < 0) {
         break;
      }

      if((pMsg = gMsgQueueHead) != NULL) {
      // we received a new message
         gMsgQueueHead = pMsg->Link;
         if(g.Verbose & VERBOSE_DUMP_RAW_RX) {
            if(g.Verbose & VERBOSE_TIMESTAMPS) {
               PrintTime(false);
            }
            LOG("Received %d byte message:\n",pMsg->MsgLen);
            DumpHex(pMsg->Msg,pMsg->MsgLen);
         }
         Cmd = pMsg->Msg[0];
         if(Cmd & CMD_RESP) {
            // This is a response to a command we sent, check the return code
            HandleResp(pMsg->Msg,pMsg->MsgLen);
         }
         else {
            HandleCmd(pMsg->Msg,pMsg->MsgLen);
         }
         free(pMsg);
      }
   // Deal with timers if we ever add any
   }

   return Ret;
}

static struct COMMAND_TABLE *FindCmd(char *CmdLine,int bSilent) 
{
   int CmdsMatched = 0;
   int MatchResult;
   char *cp;
   struct COMMAND_TABLE  *pCmd = NULL;
   struct COMMAND_TABLE  *Ret = NULL;

   cp = SkipSpaces(CmdLine);
   if(*cp) {
      pCmd = commandtable;
      while(pCmd->CmdHandler != NULL) {
         MatchResult = MatchCmd(CmdLine,pCmd->CmdString);

         if(MatchResult) {
            CmdsMatched++;
            Ret = pCmd;
         }

         if(MatchResult == 2) {
            // Exact match, no point in looking further
            Ret = pCmd;
            CmdsMatched = 1;
            break;
         }
         pCmd++;
      }

      if(CmdsMatched == 0 || 
         ((Ret->Flags & CMD_FLAG_EXACT) && MatchResult != 2)) {  // command not found or command requires an exact match and it isn't
         Ret = NULL;
         if(!bSilent) {
            printf("Error: Unknown command.\n");
         }
      }
      else if(CmdsMatched > 1) {
         if(!bSilent) {
            printf("Error: Command is ambiguous.\n");
         }
         Ret = NULL;
      }
   }

   return Ret;
}

void ParseCmd(char *CmdLine)
{
   char *cp;
   int Ret;

   cp = SkipSpaces(CmdLine);
   if(*cp) {
      if((pCurrentCmd = FindCmd(cp,false)) != NULL) {
         cp = Skip2Space(cp);
         cp = SkipSpaces(cp);
         Ret = pCurrentCmd->CmdHandler(cp);
         if(Ret == RESULT_USAGE) {
            printf("Error - Invalid argument(s)\n");
            printf("Usage:\n  %s\n",
                   pCurrentCmd->Usage != NULL ? pCurrentCmd->Usage : 
                   pCurrentCmd->HelpString);
         }
         else if(Ret == RESULT_NO_SUPPORT) {
            printf("Error - Request not supported yet\n");
         }
         else if(Ret == RESULT_BAD_ARG) {
            // Error message already displayed
         }
         else if(pCurrentCmd->Flags & CMD_FLAG_TEST) {
            // Test command, print the results
            printf("\nTest result: %s\n",Ret == 0 ? "PASS" : "FAIL");
         }
      }
   }
}

// CmdLine
// 0 = command doesn't match
// 1 = command matches
// 2 = exact command match
int MatchCmd(char *CmdLine, char *Command)
{
   CmdLine = SkipSpaces(CmdLine);

   if(!*CmdLine) {
      return 0;
   }

   while(*CmdLine && *CmdLine != ' ' && *CmdLine != '\r' && *CmdLine != '\n') {
      if(tolower(*CmdLine) != tolower(*Command)) {
         return false;
      }
      CmdLine++;
      Command++;
   }
   if(*Command == 0) {
      // Exact match
      return 2;
   }
   return 1;
}

char *SkipSpaces(char *In)
{
   while(isspace(*In)) {
      In++;
   }

   return In;
}

char *Skip2Space(char *In)
{
   while(*In != 0 && !isspace(*In)) {
      In++;
   }

   return In;
}

char *NextToken(char *In)
{
   return SkipSpaces(Skip2Space(In));
}

// Returns:
//    < 0 - error
//    = 0 - timeout
//    > 0 - Did something
int ProcessSerialData()
{
   int Ret = 0;
   int BytesRead;
   uint8_t Buf[ASYNC_MSG_BUF_LEN];

   if((BytesRead = RecvSerialData(Buf,sizeof(Buf))) > 0) {
      Ret = ParseSerialData(Buf,BytesRead);
   }

   return Ret;
}

void PrintResponse(const char *fmt, ...)
{
   va_list args;

   va_start(args,fmt);
   linenoiseHide(&gLs);
   vprintf(fmt,args);
   linenoiseShow(&gLs);
}

#define LOG(format, ...) _LOG("%s: " format,__FUNCTION__,## __VA_ARGS__)


// Returns:
//    < 0 - error
//    = 0 - timeout
//    > 0 - Did something
int ParseSerialData(uint8_t *Buf,int Len)
{
   int Ret = 0;
   int j;
   int MsgLen;
   static int RawLen = 0;

   for(j = 0; j < Len; j++) {
      MsgLen = SerialFrameIO_ParseByte(Buf[j]);
      if(MsgLen == 0) {
      // Byte wasn't part of a message
         char c = Buf[j];
      // cr, lf -> cr
         if(c == '\r') {
            c = '\n';
         }

         if(c == '\n') {
            if(RawLen != 0) {
               printf("\r\n");
            }
            RawLen = 0;
         }
         else {
         // print raw output
            if(RawLen == 0 && (g.Verbose & VERBOSE_TIMESTAMPS)) {
               PrintTime(false);
            }
            RawLen++;
            printf("%c",c);
         }
      }

      if(MsgLen > 0) {
      // we received a new message
         int MallocLen = sizeof(AsyncMsg) + MsgLen + 1;
         AsyncMsg *p;

         if(g.Verbose & VERBOSE_DUMP_RAW_MSGS) {
            LOG("Received %d byte message:\n",MsgLen);
            DumpHex(RxBuf,MsgLen);
         }

         if((p = (AsyncMsg *) malloc(MallocLen)) == NULL) {
            ELOG("Malloc of %d bytes failed\n",MallocLen);
            Ret = ENOMEM;
            break;
         }
         Ret = 1;
         p->Link = NULL;
         memcpy(p->Msg,RxBuf,MsgLen);
         p->MsgLen = MsgLen;
         p->Msg[MsgLen] = 0;

         if(gMsgQueueHead == NULL) {
            gMsgQueueHead = p;
         }
         else {
            gMsgQueueTail->Link = p;
         }
         gMsgQueueTail = p;
      }
   }

   return Ret;
}

static void GetTimeNow(struct timeval *pTime)
{
   struct timezone tz;

   gettimeofday(pTime,&tz);
}

void PrintTime(bool bCr)
{
   struct timeval ltime;
   struct tm *tm;

   GetTimeNow(&ltime);
   tm = localtime(&ltime.tv_sec);

   printf("%d:%02d:%02d.%03ld:%s",tm->tm_hour,tm->tm_min,
          tm->tm_sec,ltime.tv_usec/1000,bCr ? "\n" : " ");
}


void SerialFrameIO_SendByte(uint8_t TxByte)
{
   gTxBuf[gTxMsgLen++] = TxByte;
}

int SendAsyncMsg(uint8_t *Msg,int MsgLen)
{
   int Ret = -1;    // Assume the worse
   uint8_t Temp[ASYNC_MSG_BUF_LEN];

   do {
      if(MsgLen > gMaxMsgLen) {
         LOG("MsgLen %d is too large, max is %d\n",MsgLen,gMaxMsgLen);
         break;
      }
   // Make a local copy to ensure the buffer is writeable and that 2 bytes 
   // are available for the CRC which is appended.
      memcpy(Temp,Msg,MsgLen);

      gTxMsgLen = 0;
      if(g.Verbose & VERBOSE_DUMP_RAW_MSGS) {
         if(g.Verbose & VERBOSE_TIMESTAMPS) {
            PrintTime(false);
         }
         LOG("Sending %d bytes:\n",MsgLen);
         DumpHex(Temp,MsgLen);
      }

      SerialFrameIO_SendMsg(Temp,MsgLen);
      if(SendSerialData(gTxBuf,gTxMsgLen)) {
         Ret = 0;
      }
   } while(false);
   return Ret;
}

char *TimeStamp2String(uint32_t TAISecs)
{
   struct tm *pTm; 
   time_t Time;
   static char TimeString[80];

   if(TAISecs != 0) {
   // date -d"1/1/2000" -u +%s
   // 946684800
      Time = (time_t) (TAISecs + 946684800);
   }
   else {
   // Use the current local time if the timestamp is zero.
   // This might happen when the the node has a RTC or I2C bus failure.
      Time = time(&Time);
   }

   if((pTm = localtime(&Time)) != NULL) {
      snprintf(TimeString,sizeof(TimeString),"%d/%02d/%d %d:%02d:%02d",
              pTm->tm_mon + 1,pTm->tm_mday,pTm->tm_year + 1900,
              pTm->tm_hour,pTm->tm_min,pTm->tm_sec);
   }

   return TimeString;
}

void DisplayTime()
{
   struct tm *pTm;
   time_t Time;

   time(&Time);
   pTm = gmtime(&Time);
   printf("%2d/%02d/%d %2d:%02d:%02d UTC\n",
          pTm->tm_mon + 1,pTm->tm_mday,pTm->tm_year + 1900,
          pTm->tm_hour,pTm->tm_min,pTm->tm_sec);
   pTm = localtime(&Time);
   printf("%2d/%02d/%d %2d:%02d:%02d Local\n",
          pTm->tm_mon + 1,pTm->tm_mday,pTm->tm_year + 1900,
          pTm->tm_hour,pTm->tm_min,pTm->tm_sec);
}

void UpdateTimeNow()
{
   struct timezone tz;

   gettimeofday(&gTimeNow,&tz);
}

void SetTimeout(int Timeout,struct timeval *pTmr)
{
   long microseconds;

   UpdateTimeNow();
   microseconds = gTimeNow.tv_usec + ((Timeout % 1000) * 1000);
   pTmr->tv_usec = microseconds % 1000000;
   pTmr->tv_sec  = gTimeNow.tv_sec + (Timeout / 1000) + microseconds / 1000000;
// LOG("tv_sec: %ld, tv_usec: %ld\n",pTmr->tv_sec,pTmr->tv_usec );
}

bool IsTimedOut(struct timeval *pTmr)
{
   UpdateTimeNow();

   if(gTimeNow.tv_sec > pTmr->tv_sec ||
   (gTimeNow.tv_sec == pTmr->tv_sec && gTimeNow.tv_usec >= pTmr->tv_usec))
   {
      return true;
   }
   else {
      return false;
   }
}

struct COMMAND_TABLE *GetArgument(char *CmdLine,struct COMMAND_TABLE *pCmd) 
{
   int CmdsMatched = 0;
   int MatchResult;
   struct COMMAND_TABLE  *Ret = NULL;

   if(*CmdLine) {
      while(pCmd->CmdString != NULL) {
         if((MatchResult = MatchCmd(CmdLine,pCmd->CmdString))) {
            Ret = pCmd;
            CmdsMatched++;
            if(MatchResult == 2) {
            // Exact match, no point in looking further
               break;
            }
         }
         pCmd++;
      }
      if(CmdsMatched > 1) {
         Ret = NULL;
      }
   }
   return Ret;
}

void PrintArgList(struct COMMAND_TABLE *pCmd,bool bVerbose)
{
   if(bVerbose) {
      while(pCmd->CmdString != NULL) {
         LOG_RAW("  %s\t- %s\n",pCmd->CmdString,pCmd->HelpString);
         pCmd++;
      }
   }
   else {
      bool bFirst = true;
      LOG_RAW("[");
      while(pCmd->CmdString != NULL) {
         LOG_RAW("%s%s",bFirst ? "" : " | ",pCmd->CmdString);
         bFirst = false;
         pCmd++;
      }
      LOG_RAW("]\n");
   }
}

void TabCompletion(const char *buf, linenoiseCompletions *lc)
{
   struct COMMAND_TABLE  *pCmd = commandtable;

   while(pCmd->CmdHandler != NULL) {
      if(buf[0]== 0 || MatchCmd((char *) buf,pCmd->CmdString)) {
      // All commands if empty command line, otherwise just matched commands
         linenoiseAddCompletion(lc,pCmd->CmdString);
      }
      pCmd++;
   }
}

void RunOneCommand(char *Command)
{
   if(!gQuiet) {
      PrintHeader();
      LOG_RAW("Running \"%s\"\n",Command);
   }
   ParseCmd(Command);
}

void Sleep(int Milliseconds)
{
   unsigned long TotalUs = Milliseconds * 1000L;
   unsigned int Secs = TotalUs / 1000000L;
   useconds_t us = TotalUs % 1000000L;

   if(Secs > 0) {
      sleep(Secs);
   }
   if(us > 0) {
      usleep(us);
   }
}

char *GetHistoryPath()
{
   static char HistoryPath[128];
   char *Home = getenv("HOME");
   struct stat Stat;

   if(Home == NULL) {
      Home = ".";
   }
   snprintf(HistoryPath,sizeof(HistoryPath),"%s/.cache",Home);
   if(stat(HistoryPath,&Stat) != 0) {
   // Apparently there isn't a .cache subdirectory
      snprintf(HistoryPath,sizeof(HistoryPath),"%s/" HISTORY_FILENAME,Home);
   }
   else {
      snprintf(HistoryPath,sizeof(HistoryPath),"%s/.cache/" HISTORY_FILENAME,Home);
   }
   return HistoryPath;
}

int HelpCmd(char *CmdLine)
{
   struct COMMAND_TABLE  *pCmd = FindCmd(CmdLine,true);

   if(pCmd == NULL) {
      PrintHeader();
      printf("\n");
      pCmd = commandtable;
      while(pCmd->CmdString != NULL) {
         if(!(pCmd->Flags & CMD_FLAG_HIDE)) {
            printf("%14s - %s\n",pCmd->CmdString,pCmd->HelpString);
         }
         pCmd++;
      }
   }
   else {
      // help <cmd>
      if(!(pCmd->Flags & CMD_FLAG_HIDE)) {
         if(pCmd->Usage != NULL) {
            printf("%s %s\n",pCmd->CmdString,pCmd->Usage);
         }
         else {
            printf("%s - %s\n",pCmd->CmdString,pCmd->HelpString);
         }
      }
   }
   return 0;
}

#define SERIAL_OPEN_FLAGS O_FSYNC | O_APPEND | O_RDWR | O_NOCTTY | O_NDELAY
bool OpenSerialPort(const char *Device,int Baudrate)
{
   int fd = -1;
   int Err = 0;  // assume the best
   struct termios options;
   struct serial_struct ss;
   bool Ret = false;

   do {
      // open the port, set the stuff
      if((fd = open(Device,SERIAL_OPEN_FLAGS)) < 0) {
         ELOG("Couldn't open \"%s\": %s\n",Device,strerror(errno));
         Err = errno;
         break;
      }

      if(tcgetattr(fd,&options) < 0) {
         ELOG("Unable to read port configuration: %s\n",strerror(errno));
         Err = errno;
         break;
      }

      /* There's a bug in the FTDI FT232 driver that requires
         hardware handshaking to be toggled before data is
         transfered properly.  If you don't do this (or run minicom manually)
         received data will be all zeros and nothing will be transmitted.
       
         Although it shouldn't matter we'll only toggle hardware handshaking
         for USB connected serial ports.
      */

      if(strstr(Device,"USB") != 0) {
         options.c_cflag |= CRTSCTS;       /* enable hardware flow control */
         if(tcsetattr(fd,TCSANOW,&options) < 0) {
            ELOG("tcsetattr() failed: %s\n",strerror(errno));
            Err = errno;
            break;
         }
         options.c_cflag &= ~CRTSCTS;      /* disable hardware flow control */
         if(tcsetattr(fd,TCSANOW,&options) < 0) {
            ELOG("tcsetattr() failed: %s\n",strerror(errno));
            Err = errno;
            break;
         }
      }

      options.c_oflag &= ~OPOST;
      options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);  // raw mode
      options.c_iflag &= ~(IXON | IXOFF | IXANY);  /* disable software flow control */
      options.c_iflag |=  IGNBRK;            /* ignore break */
      options.c_iflag &= ~ISTRIP;            /* do not strip high bit */
      options.c_iflag &= ~(INLCR | ICRNL);   /* do not modify CR/NL   */
      options.c_cflag |=  (CLOCAL | CREAD);  /* enable the receiver and set local mode */
      options.c_cflag &= ~CRTSCTS;           /* disable hardware flow control */
      options.c_cflag &= ~CSTOPB;            /* 1 stop bit */

      options.c_cflag &= ~CSIZE;             /* Mask the character size bits */
      options.c_cflag |= CS8;                /* 8 data bits */
      options.c_cflag &= ~PARENB;            /* no parity */


      if(cfsetspeed(&options,Baudrate) == -1) {
         ELOG("cfsetspeed() failed: %s\n",strerror(errno));
         Err = errno;
         break;
      }

      if(tcsetattr(fd,TCSANOW,&options) < 0) {
         ELOG("tcsetattr() failed: %s\n",strerror(errno));
         Err = errno;
         break;
      }

      if(ioctl(fd,TIOCGSERIAL,&ss) < 0) {
         ELOG("ioctl(TIOCGSERIAL) failed: %s\n",strerror(errno));
      }
      else {
         ss.flags |= ASYNC_LOW_LATENCY;
         if(ioctl(fd,TIOCSSERIAL,&ss) < 0) {
            ELOG("ioctl(TIOCSSERIAL) failed: %s\n",strerror(errno));
         }
      }

      gSerialFd = fd;
      Ret = true;
   } while(false);

   if(Err != 0 && fd >= 0) {
      close(gSerialFd);
      gSerialFd = -1;
   }

   return Ret;
}

// int WaitEvent(int Timeout,ConsoleCB ConCB,SerialDataCB SerialCB)
// 
// args:
//    Timeout max time to wait for input in milliseconds
//    ConCB function to call when console input is available
//    SerialDataCB function to call when serial data is avalable
// 
// Returns:
//    < 0 - error
//    = 0 - timeout
//    > 0 - Did something
int WaitEvent(int Timeout,ConsoleCB ConCB,SerialDataCB SerialCB)
{
   fd_set ReadFdSet;
   fd_set WriteFdSet;
   struct timeval time_out;
   int Err;
   int Ret = 0;
   struct timeval Timer;

   SetTimeout(Timeout,&Timer);
   while(Ret == 0) {
      FD_ZERO(&ReadFdSet);
      FD_ZERO(&WriteFdSet);

      if(ConCB != NULL) {
         FD_SET(gLs.ifd,&ReadFdSet);
      }
      if(SerialCB != NULL) {
         FD_SET(gSerialFd,&ReadFdSet);
      }

      time_out.tv_sec = Timeout / 1000;
      time_out.tv_usec = Timeout % 1000000;

      Err = select(gSerialFd+1,&ReadFdSet,&WriteFdSet,NULL,&time_out);
      if(Err < 0) {
         if(errno != EINTR) {
            ELOG("select failed - %s\n",strerror(errno));
            Ret = errno;
            break;
         }
         else {
            continue;
         }

      }

      if(FD_ISSET(0,&ReadFdSet)) {
      // Console is ready
         if(ConCB == NULL) {
         // No call back function provided, return a timeout
            break;
         }
         char *Line = linenoiseEditFeed(&gLs);
         if(Line == NULL || Line != linenoiseEditMore) {
            Ret = ConCB(Line);
         }
         break;
      }
      if(SerialCB != NULL && FD_ISSET(gSerialFd,&ReadFdSet)) {
         Ret = SerialCB();
      }

      if(IsTimedOut(&Timer)) {
         break;
      }
   }

   return Ret;
}

// Wait for response to specified command
// Timeout = max time to wait in milliseconds
AsyncMsg *Wait4Response(uint8_t Cmd,int Timeout)
{
   AsyncMsg *pMsg = NULL;
   uint8_t Response = Cmd | CMD_RESP;
   struct timeval Timer;

   do {
      SetTimeout(Timeout,&Timer);
      if(WaitEvent(Timeout,NULL,ProcessSerialData) <= 0) {
      // Error or timeout
         if(IsTimedOut(&Timer)) {
            if(Cmd != 0) {
               LOG("Timeout waiting for %s response\n",Cmd2Str(Cmd));
            }
         }
         else {
            LOG("Some error occurred\n");
         }
         break;
      }

      if((pMsg = gMsgQueueHead) != NULL && pMsg->Msg[0] == Response) {
      // we have received the response we were waiting for
         gMsgQueueHead = pMsg->Link;
         if(g.Verbose & VERBOSE_DUMP_RAW_RX) {
            if(g.Verbose & VERBOSE_TIMESTAMPS) {
               PrintTime(false);
            }
            LOG("Received %d byte message:\n",pMsg->MsgLen);
            DumpHex(pMsg->Msg,pMsg->MsgLen);
         }
         if(pMsg->Msg[1] != 0) {
            printf("%s returned %s.\n",Cmd2Str(Cmd),Rcode2Str(pMsg->Msg[1]));
            free(pMsg);
            pMsg = NULL;
         }
         break;
      }
      if(IsTimedOut(&Timer)) {
         break;
      }
   } while(true);

   return pMsg;
}

AsyncResp *SendCmd(uint8_t *Cmd,int MsgLen,int Timeout)
{
   AsyncMsg *pResp = NULL;

   if(SendAsyncMsg(Cmd,MsgLen) == 0) {
      pResp = Wait4Response(Cmd[0],Timeout);
   }
   return (AsyncResp *) pResp;
}

bool SendSerialData(uint8_t *Buf,int Len)
{
   bool Ret = false;

   int BytesSent = write(gSerialFd,Buf,Len);
   if(BytesSent == Len) {
      Ret = true;
      if(g.Verbose & VERBOSE_DUMP_RAW_TX) {
         LOG("Sent %d raw bytes:\n");
         DumpHex(Buf,Len);
      }
   }
   else if(BytesSent == -1) {
      ELOG("Error: write failed, %s\n",strerror(errno));
   }
   else {
      ELOG("Error: short write: requested %d, wrote %d\n",Len,BytesSent);
   }

   return Ret;
}

// return bytes read or < 0 on error
int RecvSerialData(uint8_t *Buf,int BufLen)
{
   int Ret = -1;     // Assume the worse

   Ret = read(gSerialFd,Buf,BufLen);
   if(Ret < 0) {
      if(errno != EAGAIN) {
         ELOG("%s#%d: read failed: %s (%d)\n",__FUNCTION__,__LINE__,
                strerror(errno),errno);
      }
   }
   else if(Ret == 0) {
      ELOG("read returned %d\n",Ret);
   }
   else if(g.Verbose & VERBOSE_DUMP_RAW_RX) {
      if(g.Verbose & VERBOSE_TIMESTAMPS) {
         PrintTime(false);
      }
      LOG("Read %d raw bytes\n",Ret);
      DumpHex(Buf,Ret);
   }

   return Ret;
}

void _log(const char *fmt,...)
{
   va_list args;
   va_start(args,fmt);

   if(gFlags.PromptDisplayed && !gFlags.LineNoiseHidden) {
      gFlags.LineNoiseHidden = true;
      linenoiseHide(&gLs);
   }
   vprintf(fmt,args);
   va_end(args);
}

int ConvertValue(char **Arg,uint32_t *Value)
{
   char *cp = *Arg;
   char *cp1;
   int Ret = 0;   // assume the best
   bool bIsHex = false;

   do {
      cp = SkipSpaces(cp);
      if(!*cp) {
      // No value
         Ret = 1;
         break;
      }
      if(cp[0] == '0' && cp[1] == 'x') {
      // Hex arg
         bIsHex = true;
         cp += 2;
      }
      cp1 = cp;
      while(*cp1) {
         if(isspace(*cp1)) {
            cp1++;
            break;
         }
         if((bIsHex && !isxdigit(*cp1)) || (!bIsHex && !isdigit(*cp1))) {
            Ret = 1;
            break;
         }
         cp1++;
      }
      if(Ret == 1) {
         break;
      }
      if(sscanf(cp,bIsHex ? "%x" : "%u",Value) != 1) {
         Ret = 1;
      }
      *Arg = cp1;
   } while(false);

// LOG("Returning %d, Value 0x%x Arg '%s'\n",Ret,*Value,cp1);
   return Ret;
}

