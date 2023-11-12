/*
 * CC1110_spicom SPI communications driver for Explorer Board
 *
 * Adapted from rfcat code by atlas0fd00m (c) 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#ifndef _USB_H_
#define _USB_H_
 
#include "cc1111.h"
//#include <cc1110.h>
//#include "cc1110-ext.h"

#include "types.h"
#include <stdint.h>

#define SPI_BUF_LEN 516

/***************************************************************************
 * 
 * SPI encoding: 
 *
 * Master sends a 0x99 byte, followed by number of bytes that will be sent.
 * Before second byte xfer, slave loads up buffer with number of bytes
 * available. 
 * 
 */

#define USB_READ_AGAIN    ((char) -1)

// NOTE: USB and VCOM terminology is maintained to improve compatability with 
// the rest of rfact. No USB is actually used on the Explorer board. Instead, the
// communications interface is SPI with a custom transport layer.
 
// USB simulation stuff
/*#define     EP5OUT_BUFFER_SIZE      516 // data buffer size + 4

#define     EP_STATE_IDLE      0
#define     EP_STATE_TX        1
#define     EP_STATE_RX        2
#define     EP_STATE_STALL     3

typedef struct {
    u8*  INbuf;
    u16  INbytesleft;
    u8*  OUTbuf;
    u16  OUTlen;
    u8   OUTapp;
    u8   OUTcmd;
    u16  OUTbytesleft;
    volatile u8   flags;
    u8   epstatus;
    __xdata u8* dptr;
} USB_EP_IO_BUF;
 */
 
// External interface

void initUSB();
void waitForUSBsetup();
void usbProcessEvents();
void vcom_disable();
void vcom_enable();
char vcom_getchar();
void vcom_putchar(char c);
void vcom_flush();

void vcom_putstr(char* __xdata  buff);
void vcom(char* __xdata  buff);
//void vcom_up();
//void vcom_down();
void usb_up(void);
void usb_down(void);
void registerCb_ep5(int (*callback)(void));
void processOUTEP5(void);

#endif // _USB_H_
