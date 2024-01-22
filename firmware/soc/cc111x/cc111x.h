#ifndef _CC111X_H_
#define _CC111X_H_

#include <stdint.h>

#define  RFTXRX_VECTOR  0    // RF TX done / RX ready
#define  ADC_VECTOR     1    // ADC End of Conversion
#define  URX0_VECTOR    2    // USART0 RX Complete
#define  URX1_VECTOR    3    // USART1 RX Complete
#define  ENC_VECTOR     4    // AES Encryption/Decryption Complete
#define  ST_VECTOR      5    // Sleep Timer Compare
#define  P2INT_VECTOR   6    // Port 2 Inputs
#define  UTX0_VECTOR    7    // USART0 TX Complete
#define  DMA_VECTOR     8    // DMA Transfer Complete
#define  T1_VECTOR      9    // Timer 1 (16-bit) Capture/Compare/Overflow
#define  T2_VECTOR      10   // Timer 2 (MAC Timer) Overflow
#define  T3_VECTOR      11   // Timer 3 (8-bit) Capture/Compare/Overflow
#define  T4_VECTOR      12   // Timer 4 (8-bit) Capture/Compare/Overflow
#define  P0INT_VECTOR   13   // Port 0 Inputs
#define  UTX1_VECTOR    14   // USART1 TX Complete
#define  P1INT_VECTOR   15   // Port 1 Inputs
#define  RF_VECTOR      16   // RF General Interrupts
#define  WDT_VECTOR     17   // Watchdog Overflow in Timer Mode

static __idata __at (0x00) unsigned char R0;
static __idata __at (0x01) unsigned char R1;
static __idata __at (0x02) unsigned char R2;
static __idata __at (0x03) unsigned char R3;
static __idata __at (0x04) unsigned char R4;
static __idata __at (0x05) unsigned char R5;
static __idata __at (0x06) unsigned char R6;
static __idata __at (0x07) unsigned char R7;

__sbit __at (0x80) P0_0;
__sbit __at (0x81) P0_1;
__sbit __at (0x82) P0_2;
__sbit __at (0x83) P0_3;
__sbit __at (0x84) P0_4;
__sbit __at (0x85) P0_5;
__sbit __at (0x86) P0_6;
__sbit __at (0x87) P0_7;
__sbit __at (0x90) P1_0;
__sbit __at (0x91) P1_1;
__sbit __at (0x92) P1_2;
__sbit __at (0x93) P1_3;
__sbit __at (0x94) P1_4;
__sbit __at (0x95) P1_5;
__sbit __at (0x96) P1_6;
__sbit __at (0x97) P1_7;
__sbit __at (0xa0) P2_0;
__sbit __at (0xa1) P2_1;
__sbit __at (0xa2) P2_2;
__sbit __at (0xa3) P2_3;
__sbit __at (0xa4) P2_4;
__sbit __at (0xa5) P2_5;
__sbit __at (0xa6) P2_6;
__sbit __at (0xa7) P2_7;
__sbit __at (0xa8) RFTXRXIE;  // RF TX/RX FIFO interrupt enable
__sbit __at (0xa9) ADCIE;     // ADC Interrupt Enable
__sbit __at (0xaa) URX0IE;    // USART0 RX Interrupt Enable
__sbit __at (0xab) URX1IE;    // USART1 RX Interrupt Enable
__sbit __at (0xac) ENCIE;     // AES Encryption/Decryption Interrupt Enable
__sbit __at (0xad) STIE;      // Sleep Timer Interrupt Enable
__sbit __at (0xaf) EA;        // Global Interrupt Enable

__sbit __at (0xAF) IEN_EA;

__sfr __at (0x80) P0;

__sfr __at (0x86) U0CSR;
__sfr __at (0x87) PCON;
__sfr __at (0x88) TCON;
__sfr __at (0x89) P0IFG;
__sfr __at (0x8A) P1IFG;
__sfr __at (0x8B) P2IFG;
__sfr __at (0x8C) PICTL;
__sfr __at (0x8D) P1IEN;
__sfr __at (0x8F) P0INP;
__sfr __at (0x90) P1;
__sfr __at (0x91) RFIM;
__sfr __at (0x93) XPAGE;      //really called MPAGE
__sfr __at (0x93) _XPAGE;     //really called MPAGE
__sfr __at (0x95) ENDIAN;
__sfr __at (0x98) S0CON;
__sfr __at (0x9A) IEN2;

// S1CON (0x9B) - CPU Interrupt Flag 3
__sfr __at (0x9B) S1CON;
#define S1CON_RFIF_1                      0x02
#define S1CON_RFIF_0                      0x01

__sfr __at (0x9C) T2CT;
__sfr __at (0x9D) T2PR;       //used by radio for storage
__sfr __at (0x9E) TCTL;
__sfr __at (0xA0) P2;
__sfr __at (0xA1) WORIRQ;
__sfr __at (0xA2) WORCTRL;
__sfr __at (0xA3) WOREVT0;
__sfr __at (0xA4) WOREVT1;
__sfr __at (0xA5) WORTIME0;
__sfr __at (0xA6) WORTIME1;
__sfr __at (0xA8) IEN0;
__sfr __at (0xA9) IP0;
__sfr __at (0xAB) FWT;
__sfr __at (0xAC) FADDRL;
__sfr __at (0xAD) FADDRH;
__sfr __at (0xAE) FCTL;
__sfr __at (0xAF) FWDATA;
__sfr __at (0xB1) ENCDI;
__sfr __at (0xB2) ENCDO;
__sfr __at (0xB3) ENCCS;
__sfr __at (0xB4) ADCCON1;
__sfr __at (0xB5) ADCCON2;
__sfr __at (0xB6) ADCCON3;
__sfr __at (0xB8) IEN1;
__sfr __at (0xB9) IP1;
__sfr __at (0xBA) ADCL;
__sfr __at (0xBB) ADCH;
__sfr __at (0xBC) RNDL;
__sfr __at (0xBD) RNDH;
__sfr __at (0xBE) SLEEP;
__sfr __at (0xC0) IRCON;
__sfr __at (0xC1) U0DBUF;
__sfr __at (0xC2) U0BAUD;
__sfr __at (0xC4) U0UCR;
__sfr __at (0xC5) U0GCR;
__sfr __at (0xC6) CLKCON;
__sfr __at (0xC7) MEMCTR;
__sfr __at (0xC9) WDCTL;
__sfr __at (0xCA) T3CNT;
__sfr __at (0xCB) T3CTL;
__sfr __at (0xCC) T3CCTL0;
__sfr __at (0xCD) T3CC0;
__sfr __at (0xCE) T3CCTL1;
__sfr __at (0xCF) T3CC1;

__sfr __at (0xD1) DMAIRQ;
__sfr16 __at (0xD3D2) DMA1CFG;
__sfr16 __at (0xD5D4) DMA0CFG;

// DMAARM (0xD6) - DMA Channel Arm
__sfr __at (0xD6) DMAARM;
#define DMAARM_ABORT                      0x80
#define DMAARM4                           0x10
#define DMAARM3                           0x08
#define DMAARM2                           0x04
#define DMAARM1                           0x02
#define DMAARM0                           0x01

__sfr __at (0xD7) DMAREQ;
__sfr __at (0xD8) TIMIF;
__sfr __at (0xD9) RFD;
__sfr16 __at (0xDBDA) T1CC0;  //used by timer for storage
__sfr16 __at (0xDDDC) T1CC1;
__sfr16 __at (0xDFDE) T1CC2;

// RFST (0xE1) - RF Strobe Commands
__sfr __at (0xE1) RFST;
#define RFST_SFSTXON                      0x00
#define RFST_SCAL                         0x01
#define RFST_SRX                          0x02
#define RFST_STX                          0x03
#define RFST_SIDLE                        0x04
#define RFST_SNOP                         0x05

__sfr __at (0xE2) T1CNTL;
__sfr __at (0xE3) T1CNTH;
__sfr __at (0xE4) T1CTL;
__sfr __at (0xE5) T1CCTL0;
__sfr __at (0xE6) T1CCTL1;
__sfr __at (0xE7) T1CCTL2;
__sfr __at (0xE8) IRCON2;

// RFIF (0xE9) - RF Interrupt Flags
__sfr __at (0xE9) RFIF;
#define RFIF_IRQ_TXUNF                    0x80
#define RFIF_IRQ_RXOVF                    0x40
#define RFIF_IRQ_TIMEOUT                  0x20
#define RFIF_IRQ_DONE                     0x10
#define RFIF_IRQ_CS                       0x08
#define RFIF_IRQ_PQT                      0x04
#define RFIF_IRQ_CCA                      0x02
#define RFIF_IRQ_SFD                      0x01

__sfr __at (0xEA) T4CNT;
__sfr __at (0xEB) T4CTL;
__sfr __at (0xEC) T4CCTL0;
__sfr __at (0xED) T4CC0;      //used by radio for storage
__sfr __at (0xEE) T4CCTL1;
__sfr __at (0xEF) T4CC1;      //used by radio for storage

__sfr __at (0xF1) PERCFG;
__sfr __at (0xF2) ADCCFG;
__sfr __at (0xF3) P0SEL;
__sfr __at (0xF4) P1SEL;
__sfr __at (0xF5) P2SEL;
__sfr __at (0xF6) P1INP;
__sfr __at (0xF7) P2INP;
__sfr __at (0xF8) U1CSR;
__sfr __at (0xF9) U1DBUF;
__sfr __at (0xFA) U1BAUD;
__sfr __at (0xFB) U1UCR;
__sfr __at (0xFC) U1GCR;
__sfr __at (0xFD) P0DIR;
__sfr __at (0xFE) P1DIR;
__sfr __at (0xFF) P2DIR;

static __xdata __at (0xdf00) unsigned char SYNC1;
static __xdata __at (0xdf01) unsigned char SYNC0;
static __xdata __at (0xdf02) unsigned char PKTLEN;
static __xdata __at (0xdf03) unsigned char PKTCTRL1;
static __xdata __at (0xdf04) unsigned char PKTCTRL0;
static __xdata __at (0xdf05) unsigned char ADDR;
static __xdata __at (0xdf06) unsigned char CHANNR;
static __xdata __at (0xdf07) unsigned char FSCTRL1;
static __xdata __at (0xdf08) unsigned char FSCTRL0;
static __xdata __at (0xdf09) unsigned char FREQ2;
static __xdata __at (0xdf0a) unsigned char FREQ1;
static __xdata __at (0xdf0b) unsigned char FREQ0;
static __xdata __at (0xdf0c) unsigned char MDMCFG4;
static __xdata __at (0xdf0d) unsigned char MDMCFG3;
static __xdata __at (0xdf0e) unsigned char MDMCFG2;
static __xdata __at (0xdf0f) unsigned char MDMCFG1;
static __xdata __at (0xdf10) unsigned char MDMCFG0;
static __xdata __at (0xdf11) unsigned char DEVIATN;
static __xdata __at (0xdf12) unsigned char MCSM2;
static __xdata __at (0xdf13) unsigned char MCSM1;
static __xdata __at (0xdf14) unsigned char MCSM0;
static __xdata __at (0xdf15) unsigned char FOCCFG;
static __xdata __at (0xdf16) unsigned char BSCFG;
static __xdata __at (0xdf17) unsigned char AGCCTRL2;
static __xdata __at (0xdf18) unsigned char AGCCTRL1;
static __xdata __at (0xdf19) unsigned char AGCCTRL0;
static __xdata __at (0xdf1a) unsigned char FREND1;
static __xdata __at (0xdf1b) unsigned char FREND0;
static __xdata __at (0xdf1c) unsigned char FSCAL3;
static __xdata __at (0xdf1d) unsigned char FSCAL2;
static __xdata __at (0xdf1e) unsigned char FSCAL1;
static __xdata __at (0xdf1f) unsigned char FSCAL0;
static __xdata __at (0xdf23) unsigned char TEST2;
static __xdata __at (0xdf24) unsigned char TEST1;
static __xdata __at (0xdf25) unsigned char TEST0;
static __xdata __at (0xdf27) unsigned char PA_TABLE7;
static __xdata __at (0xdf28) unsigned char PA_TABLE6;
static __xdata __at (0xdf29) unsigned char PA_TABLE5;
static __xdata __at (0xdf2a) unsigned char PA_TABLE4;
static __xdata __at (0xdf2b) unsigned char PA_TABLE3;
static __xdata __at (0xdf2c) unsigned char PA_TABLE2;
static __xdata __at (0xdf2d) unsigned char PA_TABLE1;
static __xdata __at (0xdf2e) unsigned char PA_TABLE0;
static __xdata __at (0xdf2f) unsigned char IOCFG2;
static __xdata __at (0xdf30) unsigned char IOCFG1;
static __xdata __at (0xdf31) unsigned char IOCFG0;
static __xdata __at (0xdf36) unsigned char PARTNUM;
static __xdata __at (0xdf37) unsigned char VERSION;
static __xdata __at (0xdf38) unsigned char FREQEST;
static __xdata __at (0xdf39) unsigned char LQI;
static __xdata __at (0xdf3a) unsigned char RSSI;

// 0xDF3B: MARCSTATE - Main Radio Control State Machine State
static __xdata __at (0xdf3b) unsigned char MARCSTATE;
#define MARC_STATE_SLEEP                  0x00
#define MARC_STATE_IDLE                   0x01
#define MARC_STATE_VCOON_MC               0x03
#define MARC_STATE_REGON_MC               0x04
#define MARC_STATE_MANCAL                 0x05
#define MARC_STATE_VCOON                  0x06
#define MARC_STATE_REGON                  0x07
#define MARC_STATE_STARTCAL               0x08
#define MARC_STATE_BWBOOST                0x09
#define MARC_STATE_FS_LOCK                0x0A
#define MARC_STATE_IFADCON                0x0B
#define MARC_STATE_ENDCAL                 0x0C
#define MARC_STATE_RX                     0x0D
#define MARC_STATE_RX_END                 0x0E
#define MARC_STATE_RX_RST                 0x0F
#define MARC_STATE_TXRX_SWITCH            0x10
#define MARC_STATE_RX_OVERFLOW            0x11
#define MARC_STATE_FSTXON                 0x12
#define MARC_STATE_TX                     0x13
#define MARC_STATE_TX_END                 0x14
#define MARC_STATE_RXTX_SWITCH            0x15
#define MARC_STATE_TX_UNDERFLOW           0x16

static __xdata __at (0xdf3c) unsigned char PKTSTATUS;
static __xdata __at (0xdf3d) unsigned char VCO_VC_DAC;


struct DmaDescr {
   //SDCC allocates bitfields lo-to-hi
   uint8_t srcAddrHi, srcAddrLo;
   uint8_t dstAddrHi, dstAddrLo;
   uint8_t lenHi     : 5;
   uint8_t vlen      : 3;
   uint8_t lenLo;
   uint8_t trig      : 5;
   uint8_t tmode     : 2;
   uint8_t wordSize  : 1;
   uint8_t priority  : 2;
   uint8_t m8        : 1;
   uint8_t irqmask      : 1;
   uint8_t dstinc    : 2;
   uint8_t srcinc    : 2;
};

#define UCSR_ACTIVE     0x01  // USART transmit/receive active status 0:idle 1:busy
#define UCSR_TX_BYTE    0x02  // Transmit byte status 0:Byte not transmitted 1:Last byte transmitted
#define UCSR_RX_BYTE    0x04  // Receive byte status 0:No byte received 1:Received byte ready
#define UCSR_ERR        0x08  // UART parity error status 0:No error 1:parity error
#define UCSR_FE         0x10  // UART framing error status 0:No error 1:incorrect stop bit level
#define UCSR_SLAVE      0x20  // SPI master or slave mode select 0:master 1:slave
#define UCSR_RE         0x40  // UART receiver enable 0:disabled 1:enabled
#define UCSR_MODE       0x80  // USART mode select 0:SPI 1:UART

#endif
