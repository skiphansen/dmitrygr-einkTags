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
#ifndef _PROXY_MSGS_H_
#define _PROXY_MSGS_H_

// Define to use long message lengths
// #define USE_CAN_FD   1

#ifdef _MSC_VER
#pragma pack(1)
#define GCC_PACKED
#endif

#ifndef GCC_PACKED
#if defined(__GNUC__)
#define GCC_PACKED __attribute__ ((packed))
#else
#define GCC_PACKED
#endif
#endif

#define CMD_PING           1
#define CMD_PEEK           2
#define CMD_POKE           3
#define CMD_POKE_REG       4
#define CMD_STATUS         5
#define CMD_RFMODE         6
#define CMD_RESET          7
#define CMD_EEPROM_RD      8
#define CMD_EEPROM_WR      9
#define CMD_EEPROM_LEN     0x0a
#define CMD_COMM_BUF_LEN   0x0b
#define CMD_BOARD_TYPE     0x0c
#define CMD_SET_RF_REGS    0x0d
#define CMD_GET_RF_REGS    0x0e
#define CMD_LAST           CMD_BOARD_TYPE

#define CMD_STRINGS \
   "NOP", "PEEK", "POKE","POKE_REG","STATUS","RFMODE","RESET","EEPROM_RD" ,\
   "EEPROM_WR","EEPROM_LEN","COMM_BUF_LEN","BOARD_TYPE","SET_RF_REGS",\
   "GET_RF_REGS"

typedef enum {
   CMD_ERR_NONE,
   CMD_ERR_UNKNOWN_CMD,
   CMD_ERR_INVALID_ARG,
   CMD_ERR_CRC_ERR,
   CMD_ERR_INTERNAL,
   CMD_ERR_BUF_OVFL,
   CMD_ERR_LAST
} Rcodes;

#define CMD_ERR_STRINGS \
   "OK","UNKNOWN_CMD", "INVALID_ARG","CRC_ERR","INTERNAL","ERR_BUF_OVFL"

#endif   // _PROXY_MSGS_H_

