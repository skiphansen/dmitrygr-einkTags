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

#include "serial_shell.h"
#include "cmds.h"
#include "logging.h"
#include "proxy_msgs.h"
#include "linenoise.h"
#include "cc1110-ext.h"

#define DEFAULT_TO   100

#define EEPROM_WRITE_PAGE_SZ  256   // max write size & alignment
#define EEPROM_ERZ_SECTOR_SZ  4096  // erase size and alignment

#define REG_SYNC1       0xdf00
#define REG_SYNC0       0xdf01
#define REG_PKTLEN      0xdf02
#define REG_PKTCTRL1    0xdf03
#define REG_PKTCTRL0    0xdf04
#define REG_ADDR        0xdf05
#define REG_CHANNR      0xdf06
#define REG_FSCTRL1     0xdf07
#define REG_FSCTRL0     0xdf08
#define REG_FREQ2       0xdf09
#define REG_FREQ1       0xdf0a
#define REG_FREQ0       0xdf0b
#define REG_MDMCFG4     0xdf0c
#define REG_MDMCFG3     0xdf0d
#define REG_MDMCFG2     0xdf0e
#define REG_MDMCFG1     0xdf0f
#define REG_MDMCFG0     0xdf10
#define REG_DEVIATN     0xdf11
#define REG_MCSM2       0xdf12
#define REG_MCSM1       0xdf13
#define REG_MCSM0       0xdf14
#define REG_FOCCFG      0xdf15
#define REG_BSCFG       0xdf16
#define REG_AGCCTRL2    0xdf17
#define REG_AGCCTRL1    0xdf18
#define REG_AGCCTRL0    0xdf19
#define REG_FREND1      0xdf1a
#define REG_FREND0      0xdf1b
#define REG_FSCAL3      0xdf1c
#define REG_FSCAL2      0xdf1d
#define REG_FSCAL1      0xdf1e
#define REG_FSCAL0      0xdf1f
#define REG_TEST2       0xdf23
#define REG_TEST1       0xdf24
#define REG_TEST0       0xdf25
#define REG_PA_TABLE7   0xdf27
#define REG_PA_TABLE6   0xdf28
#define REG_PA_TABLE5   0xdf29
#define REG_PA_TABLE4   0xdf2a
#define REG_PA_TABLE3   0xdf2b
#define REG_PA_TABLE2   0xdf2c
#define REG_PA_TABLE1   0xdf2d
#define REG_PA_TABLE0   0xdf2e
#define REG_IOCFG2      0xdf2f
#define REG_IOCFG1      0xdf30
#define REG_IOCFG0      0xdf31
#define REG_PARTNUM     0xdf36
#define REG_VERSION     0xdf37
#define REG_FREQEST     0xdf38
#define REG_LQI         0xdf39
#define REG_RSSI        0xdf3a
#define REG_MARCSTATE   0xdf3b
#define REG_PKTSTATUS   0xdf3c

// Port 0 bits
#define P0_EPD_BS1      0x01
#define P0_TP6          0x02
#define P0_EPD_nRST     0x04
#define P0_SPI0_CLK     0x08
#define P0_SPI0_MOSI    0x10
#define P0_SPI0_MIOSO   0x20
#define P0_EPD_nENABLE  0x40
#define P0_EEPROM_nCS   0x80

// Port 1 bits
#define P1_EPD_BUSY     0x01
#define P1_EPD_CS       0x02
#define P1_EPD_RESET    0x04


typedef struct {
   size_t TblSize;
   uint8_t *pTbl;
} InitTbl;


int BoardTypeCmd(char *CmdLine);
int RadioCfgCmd(char *CmdLine);
int PingCmd(char *CmdLine);
int ResetCmd(char *CmdLine);
int RxCmd(char *CmdLine);
int EEPROM_ReadCmd(char *CmdLine);
int EEPROM_WrCmd(char *CmdLine);
int EEPROM_BackupCmd(char *CmdLine);
int EEPROM_Erase(char *CmdLine);
int EEPROM_RestoreCmd(char *CmdLine);
int DumpRfRegsCmd(char *CmdLine);
int SetRegCmd(char *CmdLine);
int SN2MACCmd(char *CmdLine);
int TxCmd(char *CmdLine);
int DumpSettingsCmd(char *CmdLine);
int PortRdWrCmd(uint8_t Port,bool bWrite,char *CmdLine);
int P0rdCmd(char *CmdLine);
int P0wrCmd(char *CmdLine);
int P1rdCmd(char *CmdLine);
int P1wrCmd(char *CmdLine);
int P2rdCmd(char *CmdLine);
int P2wrCmd(char *CmdLine);
int EEPROM_RdInternal(int Adr,FILE *fp,uint8_t *RdBuf,int Len);
int SendInitTbl(InitTbl *pTbl);
int PowerUpEPD(void);
int InitEPD(void);
int EpdBusyWait(int State,int Timeout);

// Eventual CC1101 API functions.  
// Function names based on https://github.com/LSatan/SmartRC-CC1101-Driver-Lib
int setSidle(void);
int SetTx(float mhz);

struct COMMAND_TABLE commandtable[] = {
   { "board_type",  "Display board type",NULL,0,BoardTypeCmd},
   { "dump_rf_regs", "Display settings of all RF registers",NULL,0,DumpRfRegsCmd},
   { "dump_settings", "Display EEPROM settings","dump_settings [file]",0,DumpSettingsCmd},
   { "eerd",  "Read data from EEPROM","eerd <address> <length>",0,EEPROM_ReadCmd},
   { "eewr",  "Write data to EEPROM","eewr <address> <length> <data>",0,EEPROM_WrCmd},
   { "ee_backup",  "Write EEPROM data to a file","ee_backup <path>",0,EEPROM_BackupCmd},
   { "ee_erase",  "Erase EEPROM sectors","ee_erase <address> <sectors>",0,EEPROM_Erase},
   { "ee_restore", "Read EEPROM data from a file","ee_restore <path>",0,EEPROM_RestoreCmd},
   { "ping",  "Send a ping",NULL,0,PingCmd},
   { "radio_config", "Set radio configuration",NULL,0,RadioCfgCmd},
   { "reset", "reset device",NULL,0,ResetCmd},
   { "rx", "Enter RF receive mode",NULL,0,RxCmd},
   { "p0rd", "Read port 0",NULL,0,P0rdCmd},
   { "p0wr", "Write port 0",NULL,0,P0wrCmd},
   { "p1rd", "Read port 1",NULL,0,P1rdCmd},
   { "p1wr", "Write port 1",NULL,0,P1wrCmd},
   { "p2rd", "Read port 2",NULL,0,P2rdCmd},
   { "p2wr", "Write port 2",NULL,0,P2wrCmd},
   { "set_reg", "set chip register device",NULL,0,SetRegCmd},
   { "sn2mac",  "Convert a Chroma serial number string to MAC address",NULL,0,SN2MACCmd},
   { "tx", "Send text",NULL,0,TxCmd},
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
// Modulated = false
// Modulation Format = ASK/OOK 
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
   {0xdf0e,0xb0},  // MDMCFG2: Modem Configuration 
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
// Modulated = false
// Modulation Format = ASK/OOK 
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
   {0xdf0e,0xb0},  // MDMCFG2: Modem Configuration 
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

// Test configuration used by https://github.com/nopnop2002/esp-idf-cc1101
// Address Config = No address check 
// Base Frequency = 902.000000
// CRC Enable = true 
// Carrier Frequency = 901.934937 
// Channel Number = 0 
// Channel Spacing = 199.951172 
// Data Rate = 38.3835 
// Deviation = 20.629883 
// Device Address = ff 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 61 
// Packet Length Mode = Variable packet length mode. Packet length configured by the first byte after sync word 
// Preamble Count = 4 
// RX Filter BW = 101.562500 
// Sync Word Qualifier Mode = 30/32 sync word bits detected 
// TX Power = 10 
// Whitening = false 
// The following was generated by setting the spec for Register to "{REG_@RN@,0x@VH@}," 
RfSetting gIDF_Basic[] = {
    {REG_SYNC1,0xC7},
    {REG_SYNC0,0x0A},
    {REG_PKTLEN,0x3D},
    {REG_PKTCTRL0,0x05},
    {REG_ADDR,0xFF},
    {REG_FSCTRL1,0x08},
// Base Frequency = 902.000000
    {REG_FREQ2,0x22},
    {REG_FREQ1,0xB1},
    {REG_FREQ0,0x3b},
    {REG_MDMCFG4,0xCA},
    {REG_MDMCFG3,0x83},
    {REG_MDMCFG2,0x93},
    {REG_DEVIATN,0x35},
//   {REG_MCSM0,0x18},   FS_AUTOCAL = 1, PO_TIMEOUT = 2
    {REG_MCSM0,0x18},
    {REG_FOCCFG,0x16},
    {REG_AGCCTRL2,0x43},
    {REG_FSCAL3,0xE9},
    {REG_FSCAL2,0x2A},
    {REG_FSCAL1,0x00},
    {REG_FSCAL0,0x1F},
    {REG_TEST2,0x81},
    {REG_TEST1,0x35},
    {REG_TEST0,0x09},
    {REG_PA_TABLE0,0xC0},
    {0}  // end of table
};

// RF configuration from Dimitry's orginal code 
// Address Config = No address check 
// Base Frequency = 902.999756 
// CRC Enable = true 
// Carrier Frequency = 902.999756 
// Channel Number = 0 
// Channel Spacing = 335.632324 
// Data Rate = 249.939 
// Deviation = 165.039063 
// Device Address = 22 
// Manchester Enable = false 
// Modulated = true 
// Modulation Format = GFSK 
// PA Ramping = false 
// Packet Length = 255 
// Packet Length Mode = Variable packet length mode. Packet length configured by the first byte after sync word 
// Preamble Count = 24 
// RX Filter BW = 650.000000 
// Sync Word Qualifier Mode = 30/32 sync word bits detected 
// TX Power = 0 
// Whitening = true
// Rf settings for CC1110
// The following was generated by setting the spec for Register to "{REG_@RN@,0x@VH@}," 
RfSetting gDmitry915[] = {
   {REG_ADDR,0xFF},
// Base Frequency = 902.999756 
   {REG_FREQ2,0x22},
   {REG_FREQ1,0xBB},
   {REG_FREQ0,0x13},
   {REG_MDMCFG4,0x1D},
   {REG_MDMCFG3,0x3B},
   {REG_MDMCFG2,0x13},
   {REG_MDMCFG1,0x73},
   {REG_MDMCFG0,0xA7},
   {REG_DEVIATN,0x65},
   {REG_MCSM0,0x18},
   {REG_FOCCFG,0x1E},
   {REG_BSCFG,0x1C},
   {REG_AGCCTRL2,0xC7},
   {REG_AGCCTRL1,0x00},
   {REG_AGCCTRL0,0xB0},
   {REG_FREND1,0xB6},
   {REG_FSCAL3,0xEA},
   {REG_FSCAL2,0x2A},
   {REG_FSCAL1,0x00},
   {REG_FSCAL0,0x1F},
   {REG_TEST1,0x31},
   {REG_TEST0,0x09},
   {REG_PA_TABLE0,0x8E},
   {REG_IOCFG0,0x06},
   {0}  // end of table
};

typedef struct {
   const char *Desc;
   RfSetting *Settings;
} RfModes;

RfModes gRfModes[] = {
   {"Carrier_915Mhz",g915CW},
   {"Carrier_866Mhz",g866CW},
   {"IDF_Basic",gIDF_Basic},
   {"Dmitry915",gDmitry915},
   {NULL}   // end of table
};

// <cmd_len> <cmd> <data...>
uint8_t Chroma29_C8154Init0[] = {
   5,
   0x01, // Power Setting (PWR)
   0x07,0x00,0x09,0x00,

   4, 
   0x06, // Booster Soft Start (BTST)
   0x07,0x07,0x0f,
   0
};

uint8_t Chroma29_C8154Init1[] = {
   2,
   0x00, // Panel Setting (PSR)
   0x8f, // 10 x 0 1 1 1 1
         // ^^   ^ ^ ^ ^  ^- RST_N controller not 
         //  |   | | | +---- SHD_N DC-DC converter on
         //  |   | | +------ SHL Shift right
         //  |   | +-------- UD scan up
         //  |   +---------- KWR Pixel with K/W/Red run LU1 and LU2
         //  +-------------- RES 128x296
   4,
   0x61,       //RESOLUTION SETTING (TRES)
   0x80,       // 0x80 = 128
   0x01,0x28,  // 0x128 = 296
};

uint8_t Chroma29_C8154Init2[] = {
   2,
   0x30, // PLL control (PLL)
   0x29,

   2,
   0x50, // Vcom and data interval setting (CDI)
   0x17,

   2,
   0x82, // VCM_DC Setting (VDCS)  NB: this is display dependent !
   0x08, //== -.8v

   2,
   0x60, // TCON setting (TCON)
   0x22
};

uint8_t Chroma29_C8154InitLUTC1[] = {
   16,
   0x20, // VCOM1 LUT (LUTC1)
   0x01,0x01,0x01,0x03,0x04,0x09,0x06,0x06,
   0x0A,0x04,0x04,0x19,0x03,0x04,0x09
};

uint8_t Chroma29_C8154InitLUTW[] = {
   16,
   0x21, // WHITE LUT (LUTW) (R21H)
   0x01,0x01,0x01,0x03,0x84,0x09,0x86,0x46,
   0x0A,0x84,0x44,0x19,0x03,0x44,0x09
};

uint8_t Chroma29_C8154InitLUTB[] = {
   16,
   0x22, // BLACK LUT (LUTB) (R22H)
   0x01,0x01,0x01,0x43,0x04,0x09,0x86,0x46,
   0x0A,0x84,0x44,0x19,0x83,0x04,0x09
};

uint8_t Chroma29_C8154InitLUTC2[] = {
   16,
   0x25, // VCOM2 LUT (LUTC2)
   0x0A,0x0A,0x01,0x02,0x14,0x0D,0x14,0x14,
   0x01,0x00,0x00,0x00,0x00,0x00,0x00,
};

uint8_t Chroma29_C8154InitLUTR0[] = {
   16,
   0x26, // RED0 LUT (LUTR0)
   0x4A,0x4A,0x01,0x82,0x54,0x0D,0x54,0x54,
   0x01,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma29_C8154InitLUTR1[] = {
   16,
   0x27, // RED0 LUT (LUTR1)
   0x0A,0x0A,0x01,0x02,0x14,0x0D,0x14,0x14,
   0x01,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma29_C8154InitLUTG2[] = {
   16,
   0x24, // GRAY2 LUT (LUTG2)
   0x01,0x81,0x01,0x83,0x84,0x09,0x86,0x46,
   0x0A,0x84,0x44,0x19,0x03,0x04,0x09
};

InitTbl Chroma29_C8154InitTbl[] = {
   {sizeof(Chroma29_C8154Init1),Chroma29_C8154Init1},
   {sizeof(Chroma29_C8154Init2),Chroma29_C8154Init2},
   {sizeof(Chroma29_C8154InitLUTC1),Chroma29_C8154InitLUTC1},
   {sizeof(Chroma29_C8154InitLUTW),Chroma29_C8154InitLUTW},
   {sizeof(Chroma29_C8154InitLUTB),Chroma29_C8154InitLUTB},
   {sizeof(Chroma29_C8154InitLUTC2),Chroma29_C8154InitLUTC2},
   {sizeof(Chroma29_C8154InitLUTR0),Chroma29_C8154InitLUTR0},
   {sizeof(Chroma29_C8154InitLUTR1),Chroma29_C8154InitLUTR1},
   {sizeof(Chroma29_C8154InitLUTG2),Chroma29_C8154InitLUTG2},
   {0}   // End of table
};

// #define CHROMA_42

uint8_t Chroma42_C8154Init0[] = {
   5,
   0x01, // Power Setting (PWR)
   0x07,0x00,0x09,0x00,

   4, 
   0x06, // Booster Soft Start (BTST)
   0x07,0x07,0x0f,
   0
};

uint8_t Chroma42_C8154Init1[] = {
   2,
   0x00, // Panel Setting (PSR)
   0x8f, // 10 x 1 1 1 1 1
         // ^^   ^ ^ ^ ^ ^-- RST_N controller not 
         //  |   | | | +---- SHD_N DC-DC converter on
         //  |   | | +------ SHL Shift left
         //  |   | +-------- UD scan down
         //  |   +---------- KWR Pixel with K/W/Red run LU1 and LU2
         //  +-------------- RES 128x296
   4,
   0x61,       // RESOLUTION SETTING (TRES)
   0xc8,       // 0xc8 = 200
   0x01,0x2c   // 0x12c = 300
};

uint8_t Chroma42_C8154Init2[] = {
   2,
   0x30, // PLL control (PLL)
   0x39,

   2,
   0x50, // Vcom and data interval setting (CDI)
   0x17,

   2,
   0x82, // VCM_DC Setting (VDCS)  this should NOT be a fixed value!
   0x08, // .8v which matches markings on example panel

   2,
   0x60, // TCON setting (TCON)
   0x22
};

uint8_t Chroma42_C8154InitLUTC1[] = {
   16,
   0x20, // VCOM1 LUT (LUTC1)
   0x03,0x02,0x01,0x02,0x04,0x0F,0x0A,0x0A,
   0x19,0x02,0x04,0x0F,0x00,0x00,0x00
};

uint8_t Chroma42_C8154InitLUTW[] = {
   16,
   0x21, // WHITE LUT (LUTW) (R21H)
   0x03,0x02,0x01,0x02,0x84,0x0F,0x8A,0x4A,
   0x19,0x02,0x44,0x0F,0x00,0x00,0x00
};

uint8_t Chroma42_C8154InitLUTB[] = {
   16,
   0x22, // BLACK LUT (LUTB) (R22H)
   0x03,0x02,0x01,0x42,0x04,0x0F,0x8A,0x4A,
   0x19,0x82,0x04,0x0F,0x00,0x00,0x00
};

uint8_t Chroma42_C8154InitLUTC2[] = {
   16,
   0x25, // VCOM2 LUT (LUTC2)
   0x0A,0x0A,0x01,0x02,0x14,0x08,0x14,0x14,
   0x03,0x00,0x00,0x00,0x00,0x00,0x00,
};

uint8_t Chroma42_C8154InitLUTR0[] = {
   16,
   0x26, // RED0 LUT (LUTR0)
   0x4A,0x4A,0x01,0x82,0x54,0x08,0x54,0x54,
   0x03,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154InitLUTR1[] = {
   16,
   0x27, // RED0 LUT (LUTR1)
   0x0A,0x0A,0x01,0x02,0x14,0x08,0x14,0x14,
   0x03,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154InitLUTG2[] = {
   16,
   0x24, // GRAY2 LUT (LUTG2)
   0x83,0x82,0x01,0x82,0x84,0x0F,0x8A,0x4A,
   0x19,0x02,0x04,0x0F,0x00,0x00,0x00,
};

uint8_t Chroma42_C8154DeInitLUTC1[] = {
   16,
   0x20, // VCOM1 LUT (LUTC1)
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154DeInitLUTW[] = {
   16,
   0x21, // WHITE LUT (LUTW) (R21H)
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154DeInitLUTB[] = {
   16,
   0x22, // BLACK LUT (LUTB) (R22H)
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154DeInitLUTG2[] = {
   16,
   0x24, // GRAY2 LUT (LUTG2)
   0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
   0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

uint8_t Chroma42_C8154DeInit1[] = {
   2,
   0x00, // Panel Setting (PSR)
   0x8f, // 10 x 0 1 1 1 1
         // ^^   ^ ^ ^ ^ ^-- RST_N controller not 
         //  |   | | | +---- SHD_N DC-DC converter on
         //  |   | | +------ SHL Shift left
         //  |   | +-------- UD scan down
         //  |   +---------- KWR Pixel with K/W/Red  run LU1 and LU2
         //  +-------------- RES 128x296
   4,
   0x61,       // RESOLUTION SETTING (TRES)
   0xc8,       // 0xc8 = 200
   0x01,0x2c   // 0x12c = 300
};

uint8_t Chroma42_C8154DeInit2[] = {
   5,
   0x01, // Power Setting (PWR)
   0x02,0x00,0x00,0x00,
};

uint8_t Chroma42_C8154DeInit3[] = {
   2,
   0x30, // PLL control (PLL)
   0x29,

   1,
   0x12, // Display Refresh (DRF)
};

uint8_t Chroma42_C8154DeInit4[] = {
   2,
   0x82, // VCM_DC Setting (VDCS)
   0x00,

   5,
   0x01, // Power Setting (PWR)
   0x02,0x00,0x00,0x00
};

InitTbl Chroma42_C8154InitTbl[] = {
   {sizeof(Chroma42_C8154Init1),Chroma42_C8154Init1},
   {sizeof(Chroma42_C8154Init2),Chroma42_C8154Init2},
   {sizeof(Chroma42_C8154InitLUTC1),Chroma42_C8154InitLUTC1},
   {sizeof(Chroma42_C8154InitLUTW),Chroma42_C8154InitLUTW},
   {sizeof(Chroma42_C8154InitLUTB),Chroma42_C8154InitLUTB},
   {sizeof(Chroma42_C8154InitLUTC2),Chroma42_C8154InitLUTC2},
   {sizeof(Chroma42_C8154InitLUTR0),Chroma42_C8154InitLUTR0},
   {sizeof(Chroma42_C8154InitLUTR1),Chroma42_C8154InitLUTR1},
   {sizeof(Chroma42_C8154InitLUTG2),Chroma42_C8154InitLUTG2},
   {0}   // End of table
};

InitTbl Chroma42_C8154DeInitTbl[] = {
   {sizeof(Chroma42_C8154DeInitLUTC1),Chroma42_C8154DeInitLUTC1},
   {sizeof(Chroma42_C8154DeInitLUTW),Chroma42_C8154DeInitLUTW},
   {sizeof(Chroma42_C8154DeInitLUTB),Chroma42_C8154DeInitLUTB},
   {sizeof(Chroma42_C8154DeInitLUTG2),Chroma42_C8154DeInitLUTG2},
   {sizeof(Chroma42_C8154DeInit1),Chroma42_C8154DeInit1},
   {sizeof(Chroma42_C8154DeInit1),Chroma42_C8154DeInit2},
   {sizeof(Chroma42_C8154DeInit1),Chroma42_C8154DeInit3},
   {0}   // End of table
};

InitTbl Chroma42_C8154DeInitTbl1[] = {
   {sizeof(Chroma42_C8154DeInit1),Chroma42_C8154DeInit4},
   {0}   // End of table
};

typedef int (*EpdPowerUp)(void);
typedef int (*EpdInit)(void);
typedef int (*EpdPowerDown)(void);

typedef struct {
   const char *BoardString;
   int Width;
   int Height;
   int (*EpdPowerUp)(void);
   int (*EpdInit)(void);
   int (*EpdPowerDown)(void);
} BoardInfo;

const BoardInfo gBoardInfo[] = {
   {"chroma29r",296,128},
   {"chroma42r",400,300},
   {NULL}   // End of table
};

const BoardInfo *gBoard;


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

int EEPROM_RdInternal(int Adr,FILE *fp,uint8_t *RdBuf,int Len)
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

   if(fp != NULL) {
      printf("\n");
   }

   return Ret;
}

int EEPROM_WrInternal(int Adr,FILE *fp,uint8_t *WrBuf,int Len)
{
   int Ret = RESULT_USAGE;
   uint8_t Cmd[128];
   AsyncResp *pMsg;
   int Bytes2Write;
   int BytesWritten = 0;
   int MsgLen = 0;
   int LastProgress = -1;
   int Progress = 0;

   do {
      if(WrBuf == NULL && fp == NULL) {
         ELOG("Internal error");
         break;
      }
            
      while(BytesWritten < Len) {
         Bytes2Write = (Len - BytesWritten);
         if(Bytes2Write > READ_CHUNK_LEN) {
            Bytes2Write = READ_CHUNK_LEN;
         }
         MsgLen = 0;

         Cmd[MsgLen++] = CMD_EEPROM_WR;
         Cmd[MsgLen++] = (uint8_t) (Adr & 0xff);
         Cmd[MsgLen++] = (uint8_t) ((Adr >> 8) & 0xff);
         Cmd[MsgLen++] = (uint8_t) ((Adr >> 16) & 0xff);
         if(fp != NULL) {
         // reading data from file
            if(fread(&Cmd[MsgLen],Bytes2Write,1,fp) != 1) {
               printf("fread failed - %s\n",strerror(errno));
               break;
            }
         }
         else {
         // Copy data read to buffer
            memcpy(&Cmd[MsgLen],WrBuf,Bytes2Write);
         }
         MsgLen += Bytes2Write;
         if((pMsg = SendCmd(Cmd,MsgLen,2000)) == NULL) {
            break;
         }

         Adr += Bytes2Write;
         BytesWritten += Bytes2Write;
         WrBuf += Bytes2Write;
         Progress = (100 * BytesWritten) / Len;
         if(LastProgress != Progress) {
            LastProgress = Progress;
            printf("\r%d%% complete",LastProgress);
            fflush(stdout);
         }
      }
      printf("\n");
      free(pMsg);
      Ret = RESULT_OK;
   } while(false);

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

      Ret = EEPROM_RdInternal(Adr,NULL,NULL,Len);
   } while(false);

   return Ret;
}

// eewr(adr,filename);
int EEPROM_WrCmd(char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int Adr;
   int Len;
   int EEPROM_Len = GetEEPROM_Len();
   char *cp;
   struct stat Stat;
   FILE *fp = NULL;

   do {
      if(sscanf(CmdLine,"%x",&Adr) != 1) {
         break;
      }

      if(Adr < 0 || Adr > EEPROM_Len-1) {
         LOG_RAW("Invalid address (0x%x > 0x%x)\n",Adr,EEPROM_Len-1);
         break;
      }

      if((cp = NextToken(CmdLine)) == NULL) {
         break;
      }

      if(stat(cp,&Stat) != 0) {
         LOG("stat(\"%s\") failed - %s\n",cp,strerror(errno));
         Ret = RESULT_FAIL;
         break;
      }

      if((fp = fopen(cp,"r")) == NULL) {
         LOG("fopen(\"%s\") failed - %s\n",cp,strerror(errno));
         Ret = RESULT_FAIL;
         break;
      }
      if(Len < 0 || (Adr + Stat.st_size) > EEPROM_Len) {
         PRINTF("Invalid length %d\n",Stat.st_size);
         break;
      }

      Ret = EEPROM_WrInternal(Adr,fp,NULL,Stat.st_size);
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

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
      Ret = EEPROM_RdInternal(0,fp,NULL,EEPROM_Len);
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

int EEPROM_Erase(char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int Adr;
   int Sector;
   int EEPROM_Len = GetEEPROM_Len();
   uint8_t Cmd[6] = {CMD_EEPROM_ERASE};
   AsyncResp *pMsg;

   do {
      if(sscanf(CmdLine,"%x %d",&Adr,&Sector) != 2) {
         break;
      }
      if(Adr < 0 || Adr > EEPROM_Len-1) {
         LOG_RAW("Invalid address (0x%x > 0x%x)\n",Adr,EEPROM_Len-1);
         break;
      }
      if(Sector < 0 || (Sector * 4096) > EEPROM_Len) {
         PRINTF("Invalid sector %d is invalid, must be between 0 and %d\n",
                (EEPROM_Len / 4096) - 1);
         break;
      }
      memcpy(&Cmd[1],&Adr,sizeof(uint32_t));
      Cmd[5] = (uint8_t) Sector;
      if((pMsg = SendCmd(Cmd,sizeof(Cmd),2000)) == NULL) {
         Ret = RESULT_FAIL;
         break;
      }
      free(pMsg);
      Ret = RESULT_OK;
   } while(false);

   return Ret;
}


int EEPROM_RestoreCmd(char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int EEPROM_Len = GetEEPROM_Len();
   FILE *fp = NULL;
   struct stat Stat;
   AsyncResp *pMsg;
   uint8_t Cmd[2] = {CMD_EEPROM_ERASE};

   if(*CmdLine != 0) do {
      printf("EEPROM len %dK (%d) bytes\n",EEPROM_Len / 1024,EEPROM_Len);
      if(stat(CmdLine,&Stat) != 0) {
         LOG("stat(\"%s\") failed - %s\n",CmdLine,strerror(errno));
         Ret = RESULT_FAIL;
         break;
      }

      if(EEPROM_Len != Stat.st_size) {
         LOG("Wrong file length %d, expected %d\n",Stat.st_size,EEPROM_Len);
         Ret = RESULT_FAIL;
         break;
      }

      if((fp = fopen(CmdLine,"r")) == NULL) {
         LOG("fopen(\"%s\") failed - %s\n",CmdLine,strerror(errno));
         Ret = RESULT_FAIL;
         break;
      }
      
      printf("Erasing chip...");
      pMsg = SendCmd(Cmd,1,2000);
      printf("\n");
      if(pMsg == NULL) {
         break;
      }
      free(pMsg);
      Ret = EEPROM_WrInternal(0,fp,NULL,EEPROM_Len);
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

int RxCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd[2] = {CMD_SET_RF_MODE,RFST_SRX};

   SendAsyncMsg(Cmd,sizeof(Cmd));

   return Ret;
}

int SN2MACCmd(char *CmdLine)
{
   int Ret = RESULT_BAD_ARG;  // assume the worse
   int i;
   uint8_t MacAdr[6];

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
//      MacAdr[6] = CmdLine[10];
      LOG_RAW("Stock FW MAC address: ");
      for(i = 0; i < sizeof(MacAdr); i++) {
         LOG_RAW("%s%02X",i > 0 ? ":" : "",MacAdr[i]);
      }
      LOG_RAW("\n");
      DumpHex(MacAdr,sizeof(MacAdr));
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

int TxCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd[80] = {CMD_TX_DATA};
   int MsgLen = strlen(CmdLine);

   if(MsgLen == 0 || MsgLen > sizeof(Cmd) - 1) {
      Ret = RESULT_USAGE;
   }
   else {
      memcpy(&Cmd[1],CmdLine,MsgLen);
      SendAsyncMsg(Cmd,MsgLen+1);
   }

   return Ret;
}

int BoardTypeCmd(char *CmdLine)
{
   int Ret = RESULT_OK; // Assume the best
   uint8_t Cmd[2] = {CMD_BOARD_TYPE};
   AsyncMsg *pMsg;
   char *BoardType;
   const BoardInfo *p = gBoardInfo;

   do {
      if(gBoard != NULL) {
         printf("Board type %s\n",gBoard->BoardString);
         break;
      }
      if(SendAsyncMsg(&Cmd[0],1) != 0) {
         break;
      }

      if((pMsg = Wait4Response(Cmd[0],100)) == NULL) {
         break;
      }

      BoardType = (char *) &pMsg->Msg[2];
      printf("Board type %s\n",BoardType);

      while(p->BoardString != NULL) {
         if(strcmp(BoardType,p->BoardString) == 0) {
            gBoard = p;
            break;
         }
      }

      if(gBoard == NULL) {
         printf("Unknown board type\n");
      }
      free(pMsg);
   } while(false);

   return Ret;
}

int RadioCfgCmd(char *CmdLine)
{
   uint8_t Cmd[512];
   int Ret = RESULT_OK;
   int MsgLen = 0;
   AsyncResp *pMsg;
   RfSetting *pSettings = NULL;
   bool bCW = false;

   do {
      if(*CmdLine == 0) {
         Ret = RESULT_USAGE;
         break;
      }
      *Skip2Space(CmdLine) = 0;
      printf("Searching for %s\n",CmdLine);
      for(int i = 0; gRfModes[i].Desc != NULL; i++) {
         if(strcasecmp(gRfModes[i].Desc,CmdLine) == 0) {
            pSettings = gRfModes[i].Settings;
            if(strstr(gRfModes[i].Desc,"Carrier_") != NULL) {
               bCW = true;
            }
            break;
         }
      }
      if(pSettings == NULL) {
         Ret = RESULT_USAGE;
         break;
      }
      Cmd[0] = CMD_RESET;
      if((pMsg = SendCmd(Cmd,1,DEFAULT_TO)) == NULL) {
         break;
      }
      free(pMsg);

   // Give the board time to reboot while reading the serial port
      Wait4Response(0,1000);

      Cmd[0] = CMD_PING;
      if((pMsg = SendCmd(Cmd,1,DEFAULT_TO)) == NULL) {
         break;
      }
      free(pMsg);

      Cmd[MsgLen++] = CMD_SET_RF_REGS;
      while(pSettings->Adr != 0) {
         Cmd[MsgLen++] = (uint8_t) (pSettings->Adr & 0xff);  // LSB only
         Cmd[MsgLen++] = pSettings->Value;
         pSettings++;
      }

      if((pMsg = SendCmd(Cmd,MsgLen,DEFAULT_TO)) != NULL) {
         free(pMsg);
      }

      if(bCW) {
         SetTx(0.0);
      }
   } while(false);

   if(Ret == RESULT_USAGE) {
      PRINTF("Usage: radio_config <configuration>\n");
      PRINTF("  Available configurations:\n");
      for(int i = 0; gRfModes[i].Desc != NULL; i++) {
         PRINTF("    %s\n",gRfModes[i].Desc);
      }
      Ret = RESULT_OK;
   }
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
      ELOG("mhz != 0.0 not supported yet\n");
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
   uint8_t Data[4+256];
   int Err = 0;
   int SkippedSlots = 0;
   int Ret = RESULT_OK;
   FILE *fp = NULL;
   #define PAGES_TO_SCAN   10
   uint8_t Image[PAGES_TO_SCAN * EEPROM_ERZ_SECTOR_SZ];
   uint8_t ErasedCount[256];

   do {
      memset(ErasedCount,0,sizeof(ErasedCount));
      if(*CmdLine) {
         if((fp = fopen(CmdLine,"r")) == NULL) {
            LOG("fopen(\"%s\") failed - %s\n",CmdLine,strerror(errno));
            Ret = RESULT_FAIL;
            break;
         }
         if(fread(Image,sizeof(Image),1,fp) != 1) {
            printf("fread failed - %s\n",strerror(errno));
            break;
         }
      }
      for(Page = 0; Page < PAGES_TO_SCAN; Page++) {
         Adr = Page * EEPROM_ERZ_SECTOR_SZ;
         if(fp != NULL) {
            memcpy(Data,&Image[Adr],sizeof(magicNum));
         }
         else {
            Err = EEPROM_RdInternal(Adr,NULL,Data,sizeof(magicNum));
            if(Err != 0) {
               break;
            }
         }

         if(memcmp(Data,magicNum,sizeof(magicNum)) == 0) {
            break;
         }
      }

      if(Err != 0) {
         break;
      }

      if(Page < PAGES_TO_SCAN) {
         LOG_RAW("Found setting's magic number in page %d @ 0x%x\n",Page,Adr);
         EndAdr = Adr + EEPROM_ERZ_SECTOR_SZ;
         Adr += 4;
         while(Err == 0 && Adr < EndAdr) {
            memset(Data,0xaa,sizeof(Data));
            if(fp != NULL) {
               memcpy(Data,&Image[Adr],2);
            }
            else if((Err = EEPROM_RdInternal(Adr,NULL,Data,2)) != 0) {
               break;
            }

         // first byte is type, (0xff for done), second is length
            Type = Data[0];
            Len = Data[1];

            if(Len < 1 || Len > 255) {
               LOG_RAW("Len == %d, WTF?\n",Len);
               break;
            }

            switch(Type) {
               case 0x0:
                  ErasedCount[Len]++;
                  SkippedSlots++;
                  break;

               case 0x01:  // MAC
                  Msg = "type 0x01 MAC";
                  break;

               case 0x5:  // EPD SN
                  Msg = "EPD SN";
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

               case 0x2a:  // MAC
                  Msg = "type 0x2A MAC";
                  break;

               case 0xff:
                  LOG_RAW("End of settings @ 0x%x.\n",Adr);

                  if(SkippedSlots > 0) {
                     LOG_RAW("Erased entrys:\nData Len\tCount\n");
                     for(int i = 0; i < 256; i++) {
                        if(ErasedCount[i] > 0) {
                           LOG_RAW("%d\t\t%d\n",i-2,ErasedCount[i]);
                        }
                     }
                     LOG_RAW("Total: %d\n",SkippedSlots);
                  }
                  break;

               default:
                  LOG_RAW("Unknown type 0x%x, %d bytes @ 0x%x\n",Type,Len-2,Adr);
                  break;
            }
            if(Type == 0xff) {
               break;
            }

            if(Msg != NULL) {
               LOG_RAW("Found %d byte %s @ 0x%x\n",Len-2,Msg,Adr);
               Msg = NULL;
            }

            if(Type != 0 && Len > 2) {
               if(fp != NULL) {
                  memcpy(&Data[2],&Image[Adr + 2],Len-2);
               }
               else {
                  Err = EEPROM_RdInternal(Adr + 2,NULL,&Data[2],Len-2);
                  if(Err != 0) {
                     break;
                  }
               }

               switch(Type) {
                  case 0x01:  {
                  // MAC
                     char MacString[11];
                     int i;
                     char Byte;

                     MacString[0] = Data[2];
                     MacString[1] = Data[3];
                     for(i = 0; i < 8; i++) {
                        Byte = Data[4 + (i/2)];
                        MacString[2 + i] = '0';
                        if(i & 1) {
                        // lower nibble
                           MacString[2 + i] += Byte & 0xf;
                        }
                        else {
                        // upper nibble
                           MacString[2 + i] += (Byte >> 4) & 0xf;
                        }
                     }
                     MacString[10] = 0;
                     LOG_RAW(" SN: %s? (last character is board rev)\n",MacString);
                     LOG_RAW("MAC: ");
                     DumpHex(&Data[2],Len-2);
                     break;
                  }

                  case 0x5:
                  // EPD SN
                     Data[Len] = 0;
                     LOG_RAW("EPD SN: 0x%02x, \"%s\"\n",Data[2],&Data[3]);
                     break;

                  default:
                     DumpHex(&Data[2],Len-2);
                     break;
               }
               LOG_RAW("\n");
            }
            Adr += Len;
         }
      }
   } while(false);

   if(fp != NULL) {
      fclose(fp);
   }

   return Ret;
}

int PortRdWrCmd(uint8_t Port,bool bWrite,char *CmdLine)
{
   int Ret = RESULT_USAGE;
   int Mask = 0;
   int Value = 0;
   uint8_t Cmd[4];
   AsyncResp *pMsg;

   do {
      if(bWrite) {
         if(sscanf(CmdLine,"%x %x",&Mask,&Value) != 2
            || Mask < 0 || Mask > 0xff
            || Value < 0 || Value > 0xff)
         {
            break;
         }
      }

      Cmd[0] = CMD_PORT_RW;
      Cmd[1] = Port;
      Cmd[2] = Mask;
      Cmd[3] = Value;
      if((pMsg = SendCmd(Cmd,sizeof(Cmd),2000)) == NULL) {
         Ret = RESULT_OK;
         break;
      }
      LOG_RAW("Port%d: 0x%x\n",Port,pMsg->Msg[0]);
      free(pMsg);
      Ret = RESULT_OK;
   } while(false);

   return Ret;
}

int P0rdCmd(char *CmdLine)
{
   return PortRdWrCmd(0,false,CmdLine);
}

int P0wrCmd(char *CmdLine)
{
   return PortRdWrCmd(0,true,CmdLine);
}

int P1rdCmd(char *CmdLine)
{
   return PortRdWrCmd(1,false,CmdLine);
}

int P1wrCmd(char *CmdLine)
{
   return PortRdWrCmd(1,true,CmdLine);
}

int P2rdCmd(char *CmdLine)
{
   return PortRdWrCmd(2,false,CmdLine);
}

int P2wrCmd(char *CmdLine)
{
#if 0
   return PortRdWrCmd(2,true,CmdLine);
#else
   do {
      if(gBoardInfo == NULL) {

      }
      LOG("Calling PowerUpEPD\n");
      if(PowerUpEPD() != 0) {
         break;
      }

      LOG("Calling InitEPD\n");
      if(InitEPD() != 0) {
         break;
      }
   } while(false);
   return RESULT_OK;
#endif
}

void HandleCmd(uint8_t *Msg,int MsgLen)
{
   uint8_t Cmd = Msg[0] & ~CMD_RESP;

   // Command
   switch(Cmd) {
      case CMD_RX_DATA: {
         int RSSI = Msg[1];
         if(RSSI > 127) {
            RSSI -= 256;
         }
         LOG("Received %d byte Rx frame, RSSI %d:\n",MsgLen-2,RSSI);
         DumpHex(Msg+2,MsgLen-2);
         break;
      }

      default:
         ELOG("Unexpected command received 0x%x\n",Cmd);
         break;
   }
}

int SendInitTbl(InitTbl *pTbl)
{
   uint8_t Cmd[MAX_FRAME_IO_LEN];
   AsyncResp *pMsg;
   int Ret = RESULT_OK;
   memset(Cmd,0,sizeof(Cmd));
   Cmd[0] = CMD_EPD;
// Set reset high
   Cmd[1] = EPD_FLG_DEFAULT;
#ifdef CHROMA_42
   Cmd[1] |= EPD_FLG_CS1;  // send to slave as well
#endif
   while(pTbl->TblSize) {
      memcpy(&Cmd[2],pTbl->pTbl,pTbl->TblSize);
      if((pMsg = SendCmd(Cmd,pTbl->TblSize + 2,2000)) == NULL) {
         Ret = RESULT_FAIL;
         break;
      }
      free(pMsg);
      pTbl++;
   }

   return Ret;
}

int PowerUpEPD()
{
   uint8_t Cmd[256];
   AsyncResp *pMsg;
   int Ret = RESULT_FAIL;
   int CmdLen;

   do {
      memset(Cmd,0,sizeof(Cmd));
      Cmd[0] = CMD_EPD;
   // Set reset high
      Cmd[1] = EPD_FLG_RESET | EPD_FLG_CMD;
      if((pMsg = SendCmd(Cmd,2,2000)) == NULL) {
         break;
      }
      free(pMsg);
   // reset low
      Cmd[1] &= ~EPD_FLG_RESET;
      if((pMsg = SendCmd(Cmd,2,2000)) == NULL) {
         break;
      }
      free(pMsg);
   // Release reset
      Cmd[1] |= EPD_FLG_RESET;
      if((pMsg = SendCmd(Cmd,2,2000)) == NULL) {
         break;
      }
      free(pMsg);

   // Wait for Busy to go low
      if((Ret = EpdBusyWait(0,2000)) != RESULT_OK) {
         break;
      }

      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_DEFAULT | EPD_FLG_CS1;
#ifndef CHROMA_42
      memcpy(&Cmd[CmdLen],Chroma29_C8154Init0,sizeof(Chroma29_C8154Init0));
      CmdLen += sizeof(Chroma29_C8154Init0);
#else
      memcpy(&Cmd[CmdLen],Chroma42_C8154Init0,sizeof(Chroma42_C8154Init0));
      CmdLen += sizeof(Chroma42_C8154Init0);
#endif
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);

      LOG("Sending command 4\n");
      CmdLen = 1;
      Cmd[CmdLen++] &= ~EPD_FLG_END_XFER;
      Cmd[CmdLen++] = 1;
      Cmd[CmdLen++] = 0x04;
      Cmd[CmdLen++] = 0;
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
   // Wait for Busy to go high
      if((Ret = EpdBusyWait(1,2000)) != RESULT_OK) {
         break;
      }
   // Drop CS
      CmdLen = 1;
      Cmd[CmdLen] &= ~EPD_FLG_START_XFER;
      Cmd[CmdLen++] |= EPD_FLG_END_XFER;
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
   } while(false);

   return Ret;
}


#ifndef CHROMA_42
int InitEPD()
{
   uint8_t Cmd[256];
   AsyncResp *pMsg;
   int Ret = RESULT_FAIL;
   int CmdLen = 0;

// 296 h x 128w
   #define H_SIZE 128
   #define V_SIZE 296
   uint8_t Image[V_SIZE][H_SIZE/4];
   uint8_t RedImage[V_SIZE][H_SIZE/8];
   int x;
   int y;
   int TotalBytes;
   int ImageData2Send;
   int ImageDataSent = 0;
   uint8_t *pImage;

   do {
      if(SendInitTbl(Chroma29_C8154InitTbl)) {
         break;
      }

   // 128h x 296w
   // Send Black, White and Gray image data
   // 2 bits per pixel, 4 pixels per bytes
      memset(Image,0x00,sizeof(Image));

   // Draw black line across middle
      for(x = 0; x < H_SIZE/4; x++) {
         Image[V_SIZE/2][x] = 0xff;
      }
      memset(Cmd,0,sizeof(Cmd));
      Cmd[0] = CMD_EPD;
      Cmd[1] = EPD_FLG_DEFAULT;
      Cmd[1] &= ~EPD_FLG_END_XFER;  // Send entire image as one transfer

      TotalBytes = V_SIZE * H_SIZE / 4;   // 4 pixels/byte in B&W 
      pImage = &Image[0][0];
      LOG("Sending %d bytes of B/W data\n",TotalBytes);
      while(ImageDataSent < TotalBytes) {
         ImageData2Send = TotalBytes - ImageDataSent;
      // Sending opcode + Flags + data count + image data
         if(ImageData2Send > (MAX_FRAME_IO_LEN - 3)) {
            ImageData2Send = MAX_FRAME_IO_LEN - 3;
         }
         if(ImageDataSent != 0) {
         // Command byte already sent, just send data
            Cmd[1] &= ~EPD_FLG_CMD;
         }

         CmdLen = 2;
         Cmd[CmdLen++] = (uint8_t) ImageData2Send;
         if(ImageDataSent == 0) {
            Cmd[CmdLen++] = 0x10;   // Send Display Start Transmission 1 (DTM1)
            ImageData2Send--;
         }
         memcpy(&Cmd[CmdLen],pImage,ImageData2Send);
         pImage += ImageData2Send;
         CmdLen += ImageData2Send;
         ImageDataSent += ImageData2Send;
         if(ImageDataSent == TotalBytes) {
         // Last frame, end the transfer
            Cmd[1] |= EPD_FLG_END_XFER;
         }
         if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
            break;
         }
         free(pMsg);
      }

      if(pMsg == NULL) {
         break;
      }

   // 128h x 296w
   // Send red image data 1 bits per pixel, 8 pixels per bytes
      memset(RedImage,0xff,sizeof(RedImage));

      for(y = 0; y < V_SIZE; y++) {
         RedImage[y][H_SIZE/16] &= ~1;
      }

      TotalBytes = V_SIZE * H_SIZE / 8;   // 8 pixels / byte for red
      ImageDataSent = 0;
      Cmd[0] = CMD_EPD;
      Cmd[1] = (EPD_FLG_DEFAULT & ~EPD_FLG_END_XFER);
      pImage = &RedImage[0][0];
      LOG("Sending %d bytes of Red data\n",TotalBytes);
      while(ImageDataSent < TotalBytes) {
         ImageData2Send = TotalBytes - ImageDataSent;
      // Sending opcode + Flags + data count + image data
         if(ImageData2Send > (MAX_FRAME_IO_LEN - 3)) {
            ImageData2Send = MAX_FRAME_IO_LEN - 3;
         }
         if(ImageDataSent != 0) {
         // Command byte already sent, just send data
            Cmd[1] &= ~EPD_FLG_CMD;
         }

         CmdLen = 2;
         Cmd[CmdLen++] = (uint8_t) ImageData2Send;
         if(ImageDataSent == 0) {
            Cmd[CmdLen++] = 0x13;   // Send Display Start Transmission 2 (DTM2)
            ImageData2Send--;
         }
         memcpy(&Cmd[CmdLen],pImage,ImageData2Send);
         pImage += ImageData2Send;
         CmdLen += ImageData2Send;
         ImageDataSent += ImageData2Send;
         if(ImageDataSent == TotalBytes) {
         // Last frame, end the transfer
            Cmd[1] |= EPD_FLG_END_XFER;
         }
         if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
            break;
         }
         free(pMsg);
      }

      if(pMsg == NULL) {
         break;
      }

      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_DEFAULT;
      Cmd[CmdLen++] = 1;
      Cmd[CmdLen++] = 0x12;
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);
   // Turn off the power
      EpdBusyWait(1,2000);
      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_DEFAULT;
      Cmd[CmdLen++] = 2;
      Cmd[CmdLen++] = 0x82;   // VCM_DC Setting (VDCS)
      Cmd[CmdLen++] = 0x00;   // off
      Cmd[CmdLen++] = 5;
      Cmd[CmdLen++] = 0x01;   // Power Setting (PWR)
      Cmd[CmdLen++] = 0x02;   // VDS_EN = 1, VDG_EN = RVSHLS = RVSHLS = 0
      Cmd[CmdLen++] = 0x00;   // VGHL_LV = 0 
      Cmd[CmdLen++] = 0x00;   // VDPS_LV = 0
      Cmd[CmdLen++] = 0x00;   // VDNS_LV
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);

      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_ENABLE;  // turn off power (active low)
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);

   } while(false);

   return Ret;
}
#else
int InitEPD()
{
   uint8_t Cmd[256];
   AsyncResp *pMsg;
   int Ret = RESULT_FAIL;
   int CmdLen = 0;

// 300 h x 400 w
   #define H_SIZE 400
   #define V_SIZE 300
   uint8_t Image[2][V_SIZE][H_SIZE/8];
   uint8_t RedImage[2][V_SIZE][H_SIZE/16];
   int x;
   int y;
   int TotalBytes;
   int ImageData2Send;
   int ImageDataSent = 0;
   uint8_t *pImage;

   do {
      if(SendInitTbl(Chroma42_C8154InitTbl)) {
         break;
      }

   // Send Black, White and Gray image data
   // 2 bits per pixel, 4 pixels per bytes
      memset(Image,0,sizeof(Image));

   // Draw first half of black line across middle 
      for(x = 0; x < H_SIZE/8; x++) {
         Image[0][V_SIZE/2][x] = 0xff;
      }

   // Draw second half of black line across middle 
      for(x = 0; x < H_SIZE/8; x++) {
         Image[1][V_SIZE/2][x] = 0xff;
      }

      memset(Cmd,0,sizeof(Cmd));
      Cmd[0] = CMD_EPD;
      Cmd[1] = EPD_FLG_DEFAULT;
      Cmd[1] &= ~EPD_FLG_END_XFER;  // Send entire image as one transfer

      pImage = &Image[0][0][0];
      TotalBytes = V_SIZE * H_SIZE / 8;   // 4 pixels / byte, 1/2 screen per pass
      for(int i = 0; i < 2; i++) {
         LOG("Sending %d bytes of B/W data 0x%x\n",TotalBytes,Cmd[1]);
         while(ImageDataSent < TotalBytes) {
            ImageData2Send = TotalBytes - ImageDataSent;
         // Sending opcode + Flags + data count + image data
            if(ImageData2Send > (MAX_FRAME_IO_LEN - 3)) {
               ImageData2Send = MAX_FRAME_IO_LEN - 3;
            }
            if(ImageDataSent != 0) {
            // Command byte already sent, just send data
               Cmd[1] &= ~EPD_FLG_CMD;
            }

            CmdLen = 2;
            Cmd[CmdLen++] = (uint8_t) ImageData2Send;
            if(ImageDataSent == 0) {
               Cmd[CmdLen++] = 0x10;   // Send Display Start Transmission 1 (DTM1)
               ImageData2Send--;
            }
            memcpy(&Cmd[CmdLen],pImage,ImageData2Send);
            pImage += ImageData2Send;
            CmdLen += ImageData2Send;
            ImageDataSent += ImageData2Send;
            if(ImageDataSent == TotalBytes) {
            // Last frame, end the transfer
               Cmd[1] |= EPD_FLG_END_XFER;
            }
            if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
               break;
            }
            free(pMsg);
         }

         if(pMsg == NULL) {
            break;
         }

      // Send data to the slave controller
         ImageDataSent = 0;
         Cmd[1] = EPD_FLG_DEFAULT;
         Cmd[1] &= ~EPD_FLG_END_XFER;  // Send entire image as one transfer
      // Send image data to slave
         Cmd[1] &= ~EPD_FLG_CS;
         Cmd[1] |= EPD_FLG_CS1;     
      }

   // Send red image data 1 bits per pixel, 8 pixels per bytes
      memset(RedImage,0xff,sizeof(RedImage));

      for(y = 0; y < V_SIZE; y++) {
         RedImage[1][y][0] &= ~0x80;
      }

      TotalBytes = V_SIZE * H_SIZE / 8 / 2;  // 8 pixels/byte, 1/2 screen per pass
      ImageDataSent = 0;
      Cmd[0] = CMD_EPD;
      Cmd[1] = (EPD_FLG_DEFAULT & ~EPD_FLG_END_XFER);
      pImage = &RedImage[0][0][0];
      for(int i = 0; i < 2; i++) {
         LOG("Sending %d bytes of Red data 0x%x\n",TotalBytes,Cmd[1]);
         while(ImageDataSent < TotalBytes) {
            ImageData2Send = TotalBytes - ImageDataSent;
         // Sending opcode + Flags + data count + image data
            if(ImageData2Send > (MAX_FRAME_IO_LEN - 3)) {
               ImageData2Send = MAX_FRAME_IO_LEN - 3;
            }
            if(ImageDataSent != 0) {
            // Command byte already sent, just send data
               Cmd[1] &= ~EPD_FLG_CMD;
            }

            CmdLen = 2;
            Cmd[CmdLen++] = (uint8_t) ImageData2Send;
            if(ImageDataSent == 0) {
               Cmd[CmdLen++] = 0x13;   // Send Display Start Transmission 2 (DTM2)
               ImageData2Send--;
            }
            memcpy(&Cmd[CmdLen],pImage,ImageData2Send);
            pImage += ImageData2Send;
            CmdLen += ImageData2Send;
            ImageDataSent += ImageData2Send;
            if(ImageDataSent == TotalBytes) {
            // Last frame, end the transfer
               Cmd[1] |= EPD_FLG_END_XFER;
            }
            if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
               break;
            }
            free(pMsg);
         }

         if(pMsg == NULL) {
            break;
         }

      // Send data to the slave controller
         Cmd[1] = EPD_FLG_DEFAULT | EPD_FLG_CS1;
         Cmd[1] &= ~EPD_FLG_END_XFER;  // Send entire image as one transfer
         Cmd[1] &= ~EPD_FLG_CS;     // Send entire image as one transfer
      // reset count
         ImageDataSent = 0;
      }

   // Enable cascade clock
      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_DEFAULT | EPD_FLG_ENABLE;
      Cmd[CmdLen++] = 2;
      Cmd[CmdLen++] = 0xe0;
      Cmd[CmdLen++] = 0x01;
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);

      CmdLen = 1;
      Cmd[CmdLen++] = EPD_FLG_DEFAULT | EPD_FLG_CS1;
      Cmd[CmdLen++] = 1;
      Cmd[CmdLen++] = 0x12;
      if((pMsg = SendCmd(Cmd,CmdLen,2000)) == NULL) {
         break;
      }
      free(pMsg);
      EpdBusyWait(1,2000);

   // Turn off the power
      if(SendInitTbl(Chroma42_C8154DeInitTbl)) {
         break;
      }

      EpdBusyWait(1,2000);
      if(SendInitTbl(Chroma42_C8154DeInitTbl1)) {
         break;
      }
   } while(false);

   return Ret;
}
#endif


int EpdBusyWait(int State,int Timeout)
{
   int Ret = RESULT_OK;
   AsyncResp *pMsg;
   bool bFirst = true;
   uint8_t Cmd[4] = {CMD_PORT_RW,1,0,0};
   uint8_t Busy;

   while(true) {
      if((pMsg = SendCmd(Cmd,sizeof(Cmd),2000)) == NULL) {
         LOG_RAW("Timeout\n");
         Ret = RESULT_TIMEOUT;
         break;
      }
      Busy = pMsg->Msg[0] & P1_EPD_BUSY;
      free(pMsg);
      if(Busy == State) {
         if(!bFirst) {
            LOG_RAW("\n");
         }
         break;
      }

      if(bFirst) {
         bFirst = false;
         LOG_RAW("Waiting for Busy to go %s...",State ? "high" : "low");
         fflush(stdout);
      }
   }

   return Ret;
}

void Usage()
{
   PRINTF("Usage: chroma_shell [options]\n");
   PRINTF("  options:\n");
   PRINTF("\t-b<baud rate>\tSet serial port baudrate (default %d)\n",DEFAULT_BAUDRATE);
   PRINTF("\t-c<command>\tRun specified command and exit\n");
   PRINTF("\t-d\t\tDebug mode\n");
   PRINTF("\t-D<path>\tSet path to async device (default %s)\n",gDevicePath);
   PRINTF("\t-q\t\tquiet\n");
   PRINTF("\t-v?\t\tList available verbose display levels\n");
   PRINTF("\t-v<bitmap>\tSet desired display levels (Hex bit map)\n");
}



