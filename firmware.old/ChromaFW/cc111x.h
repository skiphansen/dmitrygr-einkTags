#ifndef _CC111X_H_
#define _CC111X_H_

#include <stdint.h>

//this is needed by the compiler


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
__sfr __at (0x93) XPAGE;		//really called MPAGE
__sfr __at (0x93) _XPAGE;		//really called MPAGE
__sfr __at (0x95) ENDIAN;
__sfr __at (0x98) S0CON;
__sfr __at (0x9A) IEN2;
__sfr __at (0x9B) S1CON;
__sfr __at (0x9C) T2CT;
__sfr __at (0x9D) T2PR;			//used by radio for storage
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
__sfr __at (0xD6) DMAARM;
__sfr __at (0xD7) DMAREQ;
__sfr __at (0xD8) TIMIF;
__sfr __at (0xD9) RFD;
__sfr16 __at (0xDBDA) T1CC0;	//used by timer for storage
__sfr16 __at (0xDDDC) T1CC1;
__sfr16 __at (0xDFDE) T1CC2;

__sfr __at (0xE1) RFST;
__sfr __at (0xE2) T1CNTL;
__sfr __at (0xE3) T1CNTH;
__sfr __at (0xE4) T1CTL;
__sfr __at (0xE5) T1CCTL0;
__sfr __at (0xE6) T1CCTL1;
__sfr __at (0xE7) T1CCTL2;
__sfr __at (0xE8) IRCON2;
__sfr __at (0xE9) RFIF;
__sfr __at (0xEA) T4CNT;
__sfr __at (0xEB) T4CTL;
__sfr __at (0xEC) T4CCTL0;
__sfr __at (0xED) T4CC0;		//used by radio for storage
__sfr __at (0xEE) T4CCTL1;
__sfr __at (0xEF) T4CC1;		//used by radio for storage

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
static __xdata __at (0xdf3b) unsigned char MARCSTATE;
static __xdata __at (0xdf3c) unsigned char PKTSTATUS;
static __xdata __at (0xdf3d) unsigned char VCO_VC_DAC;


struct DmaDescr {
	//SDCC allocates bitfields lo-to-hi
	uint8_t srcAddrHi, srcAddrLo;
	uint8_t dstAddrHi, dstAddrLo;
	uint8_t lenHi		: 5;
	uint8_t vlen		: 3;
	uint8_t lenLo;
	uint8_t trig		: 5;
	uint8_t tmode		: 2;
	uint8_t wordSize	: 1;
	uint8_t priority	: 2;
	uint8_t m8			: 1;
	uint8_t irqmask		: 1;
	uint8_t dstinc		: 2;
	uint8_t srcinc		: 2;
};

#endif
