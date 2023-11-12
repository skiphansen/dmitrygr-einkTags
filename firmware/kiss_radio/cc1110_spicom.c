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

#include "cc1110_spicom.h"


#ifdef SDCC
  __code u8 sdccver[] = "SDCCv" QUOTE(SDCC);
#else 
  #ifdef __SDCC
    __code u8 sdccver[] = "SDCCv" QUOTE(__SDCC);
  #else
    __code u8 sdccver[] = "NON-SDCC";
  #endif
#endif
// BUILD_VERSION is passed in -D from Makefile

#if defined(AURA21) 
__code u8 buildname[] = {
    'A','U','R','A','2','1',' ',' ',' ','r',ASCII_LONG(BUILD_VERSION),'\x00',
};
#endif


//volatile uint8_t __xdata spi_input_buf[SPI_BUF_LEN];
volatile uint16_t input_size = 0;
volatile uint16_t input_head_idx = 0;
volatile uint16_t input_tail_idx = 0;

volatile uint8_t __xdata spi_output_buf[SPI_BUF_LEN];
volatile uint16_t output_size = 0;
volatile uint16_t output_head_idx = 0;
volatile uint16_t output_tail_idx = 0;

volatile uint16_t serial_data_available;

#define SPI_MODE_WAIT 0
#define SPI_MODE_SIZE 1
#define SPI_MODE_XFER 2
volatile uint8_t spi_mode;

volatile uint16_t master_send_size = 0;
volatile uint16_t slave_send_size = 0;

// USB simulation stuff
__xdata u8  usb_ep5_OUTbuf[EP5OUT_BUFFER_SIZE];                   // these get pointed to by the above structure
__xdata USB_EP_IO_BUF     ep5;

__xdata int (*cb_ep5)(void);



// TODO: in rx1_isr, parse packets and put them into ep5 
// (see chipcon_usb.c handleOUTEP5 fcn for ideas)

// get len and put it into ep5.OUTlen
// get data and put it into ep5.OUTbuf (first two bytes should be \x40\xe0)
// also set ep5.OUTapp (how?)
// also set ep5.OUTcmd (how?)
// set and check flags


// todo: implement txdatai() (see appFHSSNIC for details)

/***************************************************************************
 * 
 * SPI encoding: 
 *
 * Master sends a 0x99 byte, followed by number of bytes that will be sent.
 * Before second byte xfer, slave loads up buffer with number of bytes
 * available. 
 * 
 */

void rx1_isr(void) __interrupt URX1_VECTOR {
  uint8_t value;
  value = U1DBUF;

  if (spi_mode == SPI_MODE_WAIT && value == 0x99) {
    slave_send_size = output_size;
    
    spi_mode = SPI_MODE_SIZE;
    U1DBUF = slave_send_size;
    return;
  }

  if (spi_mode == SPI_MODE_SIZE) {
    master_send_size = value;
	input_size = 0;
    ep5.OUTlen = value - 2; // first two bytes are app and cmd
    if (master_send_size > 0 || slave_send_size > 0) {
      spi_mode = SPI_MODE_XFER;
    } else {
      spi_mode = SPI_MODE_WAIT;
    }
    return;
  }

  if (spi_mode == SPI_MODE_XFER && input_size < master_send_size) {
    if (input_size == 0) {
        // first byte is app
        ep5.OUTapp = value;
        //ep5.OUTbuf[0] = 0x40; // backwards compatibility
    } else if (input_size == 1) {
        // second byte is cmd
        ep5.OUTcmd = value;
        //ep5.OUTbuf[1] = 0xe0; // backwards compatibility
    } else {
        // data
        ep5.OUTbuf[input_size - 2] = value;
    }
    input_size++;
    if (input_size == master_send_size) {
        master_send_size = 0;
        serial_data_available = 1;
    }
    if (slave_send_size == 0 && master_send_size == 0) {
        spi_mode = SPI_MODE_WAIT;
    }
	
	if (serial_data_available)
    {
		ep5.flags |= EP_OUTBUF_WRITTEN;
		LED_RED = 1;
		processOUTEP5();
        // TODO: non-zero return may be needed later
        serial_data_available = 0;
    }
  }
}

void tx1_isr(void) __interrupt UTX1_VECTOR {
  IRCON2 &= ~BIT2; // Clear UTX1IF
  if (spi_mode == SPI_MODE_SIZE || spi_mode == SPI_MODE_XFER) {
    if (slave_send_size > 0 && output_size > 0) {
      slave_send_size--;
      if (slave_send_size == 0 && master_send_size == 0) {
        spi_mode = SPI_MODE_WAIT;
      }
      U1DBUF = spi_output_buf[output_tail_idx];
      output_size--;
      if (output_size == 0) {
          ep5.flags &= ~EP_INBUF_WRITTEN;
      }
      output_tail_idx++;
      if (output_tail_idx >= SPI_BUF_LEN) {
        output_tail_idx = 0;
      }
    } else {
      U1DBUF = 0x99;
    }
  } else {
    U1DBUF = 0x99;
  }
}

void vcom_flush()
{
  // dummy fcn
  return;
}

void vcom_putchar(char c)
{
  if (output_size >= SPI_BUF_LEN) {
    // drop oldest byte
    output_size--;
    output_tail_idx++;
    if (output_tail_idx >= SPI_BUF_LEN) {
      output_tail_idx = 0;
    }
  }
  spi_output_buf[output_head_idx] = c;
  
  output_head_idx++;
  if (output_head_idx >= SPI_BUF_LEN) {
    output_head_idx = 0;
  }
  output_size++;
  
  ep5.flags |= EP_INBUF_WRITTEN;
}

char vcom_pollchar()
{
  //if (serial_data_available == 0) {
    return USB_READ_AGAIN;
  //}
  //return spi_input_buf[input_tail_idx];
}

char vcom_getchar()
{
  //uint8_t s_data;
  
  //if (serial_data_available == 0) {
    return USB_READ_AGAIN;
  //}
  /*
  s_data = spi_input_buf[input_tail_idx];
  input_tail_idx++;
  if (input_tail_idx >= SPI_BUF_LEN) {
    input_tail_idx = 0;
  }
  input_size--;
  if (input_size == 0) {
    serial_data_available = 0;
  }
  return s_data;*/
}

void vcom_enable()
{
  TCON &= ~BIT3; // Clear URX1IF
  URX1IE = 1;    // Enable URX1IE interrupt

  IRCON2 &= ~BIT2; // Clear UTX1IF
  IEN2 |= BIT3;    // Enable UTX1IE interrupt
}

void vcom_disable()
{
  TCON &= ~BIT3; // Clear URX1IF
  URX1IE = 0;    // Disable URX1IE interrupt

  IRCON2 &= ~BIT2; // Clear UTX1IF
  IEN2 &= ~BIT3;    // Disable UTX1IE interrupt
}

void initUSB()
{
#if defined(AURA21) 
  /*
   * Setup slave SPI on USART1, Alt 1
   *
   * (SS): P0_2
   *  (C): P0_3
   * (MO): P0_4
   * (MI): P0_5
   */

  // Donn 12.01.2019 swap USARTs for Aura21 board
  // So -> USART1 Alternative 1 => Port P0 (PERCFG.U1CFG = 0)
  //       USART0 Alternative 2 => Port P1 (PERCFG.U0CFG = 1)
  PERCFG = (PERCFG & ~PERCFG_U1CFG) | PERCFG_U0CFG;
 
  // Set pins 2, 3 and 5 as peripheral I/O and pin 4 as GPIO output
  P0SEL = P0SEL | BIT2 | BIT3 | BIT4 | BIT5;
  P0DIR = P0DIR & ~(BIT2 | BIT3 | BIT4 | BIT5);
#endif
  /***************************************************************************
   * Configure SPI
   */

  // Set USART to SPI mode and Slave mode
  U1CSR = (U1CSR & ~U1CSR_MODE) | U1CSR_SLAVE;

  // Set:
  // - mantissa value
  // - exponent value
  // - clock phase to be centered on first edge of SCK period
  // - negative clock polarity (SCK low when idle)
  // - bit order for transfers to LSB first

// These values will give a baud rate of approx. 62.5kbps for 26 MHz clock
#define SPI_BAUD_M 59
#define SPI_BAUD_E 11

  U1BAUD = SPI_BAUD_M;
  U1GCR = (U1GCR & ~(U1GCR_BAUD_E | U1GCR_CPOL | U1GCR_CPHA | U1GCR_ORDER))
        | SPI_BAUD_E;

  TCON &= ~BIT3; // Clear URX1IF
  URX1IE = 1;    // Enable URX1IE interrupt

  IRCON2 &= ~BIT2; // Clear UTX1IF
  IEN2 |= BIT3;    // Enable UTX1IE interrupt
  
  ep5.OUTbuf = usb_ep5_OUTbuf;

  spi_mode = SPI_MODE_WAIT;

  
  
}

void usbProcessEvents()
{    
    return; /* dummy function */
}

void vcom_readline(char* buff) {
  char c;
  while ((c = vcom_getchar()) != '\n') {
    *buff++ = c;
  }
  *buff = 0;
}

void vcom_putstr(char* __xdata buff) {
  while (*buff) {
      vcom_putchar(*buff++);
  }
  vcom_flush();
}

void usb_up() {
  // dummy fcn
  vcom_enable();
}

void vcom_down() {
  // dummy fcn
  vcom_disable();
}

int txdata(u8 app, u8 cmd, u16 len, __xdata u8* dataptr)
{
    u16 test = 0;
    /*removes warning */    
    test = len; //app = cmd = len;
    
    vcom_putchar(app);
    vcom_putchar(cmd);
	vcom_putchar((u8)len);
	vcom_putchar((u8)len >> 8);
    
    /* function from usb thing, only need data ptr */    
    while (len > 0) //*dataptr) 
    {
        vcom_putchar(*dataptr++);
		len--;
    }
    vcom_putchar(0);
    vcom_flush();
    
    // TODO: may want to handle ep5.flags EP_INBUF_WRITTEN
    
    return 0;
}

// This interrupt is shared with port 2,
// so when we hook that up, fix this
void usbIntHandler(void) __interrupt P2INT_VECTOR
{
  return;
}

void p0IntHandler(void) __interrupt P0INT_VECTOR
{
    return;
}

void waitForUSBsetup()
{
    return;
}

void registerCb_ep5(int (*callback)(void))
{
    cb_ep5 = callback;
}

void appReturn(__xdata u8 len, __xdata u8* __xdata  response)
{
    ep5.flags &= ~EP_OUTBUF_WRITTEN;                       // this should be superfluous... but could be causing problems?
    txdata(ep5.OUTapp,ep5.OUTcmd, len, response);
}

void processOUTEP5(void)
{
    u16 loop;
    __xdata u8* __xdata  ptr; 

    // if the buffer is still being loaded or just plain empty, ignore this  (superfluous... may remove this check later)
    if ((ep5.flags & EP_OUTBUF_WRITTEN) == 0)
        return;

    ptr = &ep5.OUTbuf[2]; // skip first two bytes (they're superfluous)
	ep5.OUTlen -= 2;
    // system application
    if (ep5.OUTapp == 0xff)                                        
    {

        switch (ep5.OUTcmd)
        {
            case CMD_PEEK:
				ep5.OUTbytesleft =  *ptr++;
                ep5.OUTbytesleft += (u16)*ptr++ << 8;

                loop =  (u16)*ptr++;
                loop += (u16)*ptr++ << 8;
                ptr = (__xdata u8*) loop;

                txdata(ep5.OUTapp, ep5.OUTcmd, ep5.OUTbytesleft, ptr);
                ep5.OUTbytesleft = 0;
                break;

            case CMD_POKE:
                loop =  *ptr++;
                loop += *ptr++ << 8;
                ep5.dptr = (__xdata u8*) loop;

                loop = ep5.OUTlen - 2;

                for (;loop>0;loop--)
                {
                    *ep5.dptr++ = *ptr++;
                }

                //if (ep5.OUTbytesleft == 0)
                txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata u8*)&(ep5.OUTbytesleft));
                break;

            case CMD_POKE_REG:
                if (!(ep5.flags & EP_OUTBUF_CONTINUED))
                {
                    loop =  *ptr++;
                    loop += *ptr++ << 8;
                    ep5.dptr = (__xdata u8*) loop;
                }
                // FIXME: do we want to DMA here?
                
                loop = ep5.OUTbytesleft;
                if (loop > EP5OUT_MAX_PACKET_SIZE)
                {
                    loop = EP5OUT_MAX_PACKET_SIZE;
                }

                ep5.OUTbytesleft -= loop;
                //debughex16(loop);

                for (;loop>0;loop--)
                {
                    *ep5.dptr++ = *ptr++;
                }

                txdata(ep5.OUTapp, ep5.OUTcmd, 2, (__xdata u8*)&(ep5.OUTbytesleft));

                break;
            case CMD_PING:
                blink(2,2);
                txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);
                break;

            case CMD_STATUS:
                txdata(ep5.OUTapp, ep5.OUTcmd, 13, (__xdata u8*)"UNIMPLEMENTED");
                // unimplemented
                break;

            case CMD_GET_CLOCK:
                txdata(ep5.OUTapp, ep5.OUTcmd, 4, (__xdata u8*)clock);
                break;

            case CMD_BUILDTYPE:
                txdata(ep5.OUTapp, ep5.OUTcmd, sizeof(buildname), (__xdata u8*)&buildname[0]);
                break;

            case CMD_COMPILER:
                txdata(ep5.OUTapp, ep5.OUTcmd, sizeof(sdccver), (__xdata u8*)&sdccver[0]);
                break;
                
            case CMD_RFMODE:
                switch (*ptr++)
                {
                    case RFST_SRX:
                        RxMode();
                        break;
                    case RFST_SIDLE:
                        LED = 0;
                        IdleMode();
                        break;
                    case RFST_STX:
                        TxMode();
                        break;
                }
                //appReturn(ep5.OUTlen,buf);
                txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);
                break;

            case CMD_PARTNUM:
                ep5.OUTbytesleft = 1;

                ptr = (__xdata u8*) &PARTNUM;

                txdata(ep5.OUTapp, ep5.OUTcmd, ep5.OUTbytesleft, ptr);
                ep5.OUTbytesleft = 0;
                break;

            case CMD_RESET:
                if (strncmp(ptr, "RESET_NOW", 9))
                    break;   //didn't match the signature.  must have been an accident.

                // implement a RESET by trigging the watchdog timer
                WDCTL = 0x80;   // Watchdog ENABLE, Watchdog mode, 1s until reset

                txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);
                break;

            case CMD_CLEAR_CODES:
                lastCode[0] = 0;
                lastCode[1] = 0;
                //txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);   // FIXME: need to reorient all these to return LCE_NO_ERROR unless error.
                appReturn(2, ptr);
                break;

            default:
                txdata(ep5.OUTapp,ep5.OUTcmd,ep5.OUTlen,ptr);
        }

        ep5.flags &= ~EP_OUTBUF_WRITTEN; 
    }
    else
    {
        if (cb_ep5)
        {
            if (! cb_ep5())
            {
                // if the callback returns 0, we're done.  
                // if non-zero, we can't handle it right now, keep it around
                ep5.flags &= ~EP_OUTBUF_WRITTEN; 
            }
        }
        else
        {
            ep5.flags &= ~EP_OUTBUF_WRITTEN; 
        }
    }
}
