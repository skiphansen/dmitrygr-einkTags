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
#define CMD_SET_RF_MODE    0x0f
#define CMD_RX_DATA        0x10
#define CMD_TX_DATA        0x11
#define CMD_EPD            0x12
#define CMD_PORT_RW        0x13
#define CMD_EEPROM_ERASE   0x14
#define CMD_LAST           CMD_EEPROM_ERASE

#define CMD_STRINGS \
   "NOP", "PEEK", "POKE","POKE_REG","STATUS","RFMODE","RESET","EEPROM_RD" ,\
   "EEPROM_WR","EEPROM_LEN","COMM_BUF_LEN","BOARD_TYPE","SET_RF_REGS",\
   "GET_RF_REGS","SET_RF_MODE","RX_DATA","TX_DATA","EPD","PORT_RW",\
   "EEPROM_ERASE"

typedef enum {
   CMD_ERR_NONE,
   CMD_ERR_UNKNOWN_CMD,
   CMD_ERR_INVALID_ARG,
   CMD_ERR_CRC_ERR,
   CMD_ERR_INTERNAL,
   CMD_ERR_BUF_OVFL,
   CMD_ERR_TIMEOUT,
   CMD_ERR_BUSY,
   CMD_ERR_FAILED,
   CMD_ERR_LAST
} Rcodes;

#define CMD_ERR_STRINGS \
   "OK","UNKNOWN_CMD", "INVALID_ARG","CRC_ERR","INTERNAL","ERR_BUF_OVFL",\
   "TIMEOUT","BUSY","_FAILED"

// CMD_EPD argument
#define EPD_FLG_SEND_RD       0x01  // Send data read
#define EPD_FLG_CMD           0x02  // Clear Cmd/Data bit (i.e. byte is command)
#define EPD_FLG_RESET         0x04  // Set reset bit
#define EPD_FLG_ENABLE        0x10  // Set enable BS1 bit
#define EPD_FLG_START_XFER    0x20  // Activate nCS before sending data
#define EPD_FLG_END_XFER      0x40  // Deactivate nCS after sending data

#define EPD_FLG_DEFAULT       ( EPD_FLG_CMD \
                              | EPD_FLG_ENABLE \
                              | EPD_FLG_START_XFER \
                              | EPD_FLG_END_XFER)

#define EEPROM_ERASE_SECTOR   0
#define EEPROM_ERASE_BLOCK    1
#define EEPROM_ERASE_CHIP     2


#endif   // _PROXY_MSGS_H_

