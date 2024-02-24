# Displaydata Chroma (Aeon) Series Information

## Displaydata Chroma Aeon Series Specs

Three colors, either black, white, and red (BWR) or black, white, and yellow (BWY).

There is no known way to determine if a display is BWR or BWY from the serial
or model number.

| Model | Screen size | Resolution | EEPROM<br>size | Supported | [OEPL](https://github.com/jjwbruijn/OpenEPaperLink/tree/master)<br>support planned |
| -| -| - | :-: | :-: | :-: |
| Chroma 16 | 1.08 x 1.08 in | 152 x 152 |? | N | N |
| Chroma 21 | 1.9 x 1.00  in | 212 x 104 |? | N | N |
| Chroma 27 | 2.4 x 1.20  in | 296 x 152 |? | N | N |
| [Chroma 29](Chroma29.md) | 2.6 x 1.10  in | 296 x 128 | 128K | Y<br>(some versions) | Y |
| Chroma 37 | 3.2 x 1.85  in | 416 x 240 |? | N | N |
| [Chroma 42](Chroma42.md) | 3.3 x 2.50  in | 400 x 300 | 1024K | N | Y |
| Chroma 60 | 4.7 x 3.50  in | 648 x 480 |? | N | maybe |
| Chroma 74 | 6.4 x 3.50  in | 640 x 384 | 1024K | Y | Y |
| [Chroma 74H+](Chroma74H_Plus.md) | 6.4 x 3.90  in | 800 x 480 | 1024K | N | maybe |
| Chroma 125 | 10.0 x 7.50  in | 1304 x 984 | | N | N |

The displays with OEPL support marked as "maybe" are a function of supply.
I don't currently have enough of these display types to make it worth the 
effort, but that could change.

The displays with OEPl support marked as "N" means that I don't have any so
I can't add support.  

All of the orignal Chroma tags used the same basic design.  To add support
for a new model it is likely that only the EPD driver will need to be changed.

## Displaydata Aura Aeon Series Specs

Black and white only.

| Model | Screen size | Resolution | EEPROM<br>size | Supported | [OEPL](https://github.com/jjwbruijn/OpenEPaperLink/tree/master)<br>support planned |
| -| -| - | :-: | :-: | :-: |
| Aura 21 | 1.9 x 1.00  in | 212 x 104 |? | N | N |
| Aura 29 | 2.6 x 1.10  in | 296 x 128 | ? | N | N |
| Aura 29F | 2.6 x 1.10  in | 296 x 128 | ? | N | N |
| Aura 42 | 3.3 x 2.50  in | 400 x 300 | ? | N | N |

## RF configuration used by Dmitry's code

250 Kbps, GFSK with data whitening.<br>
Tx deviation: 165KHz<br>
Rx bandwith: 650Khz

Packets are variable length, CRC16 will be provided by the radio.

The system is configured to operate on one of 25 equally spaced channels from 
903MHz to 927MHz

| Channel | Frequency |
| :-: | - |
|1 | 902.999756|
|2 | 904.006653|
|3| 905.013550|
|...||
|25| 927.165283|

## USART usage

USART0 EPD SPI0 interface in alt. 2 mode which connects the SPI0 device to P2.

USART1 shared dynamically between SPI flash (EEPROM) and debug serial port.


## CC1110F32 Memory map

32k program FLASH + 4k SRAM

| Adr | Usage |
| - | - |
|0xff00-0xffff   |256 bytes fast access RAM|
|0xf000-0xfeff   |4k - 256 bytes Slow access SRAM|
|0xdf80-0xdfff   |Hardware SFR registers|
|0xdf00-0xdf7f   |Hardware Radio & I2S registers|
|0x0000-0x7fff   |Flash|


## CC1110F pin connections

The CC1110F pin connections are assumed to be common amoung all Chroma Tags,
but only Chroma74 and Chroma29 have verified.  

Note: there are some variations between different board revisions.

| Pin | Pin name | usage | Notes |
| - | - | -|-|
| 1| P1.2 | EPD RESET | display pin 10
| 2| DVDD ||
| 3| P1.1 | EPD nCS | display pin 12|
| 4| P1.0 | EPD BUSY | display pin 9|
| 5| P0.0 | EPD BS1 | display pin 8|
| 6| P0.1 | TP6/TP16 ||
| 7| P0.2 | EPD nCS1 [See note 3](#notes) | display pin 1|
| 8| P0.3 | SPI EEPROM CLK|
| 9| P0.4 | SPI EEPROM MOSI|
| 10| DVDD
| 11| P0.5 | SPI EEPROM MISO|
| 12| P0.6 | EPD nEnable| [See note 2](#notes)
| 13| P0.7 | EPD D/nC | display pin 11|
| 14| P2.0 | EEPROM nCS ||
| 15| P2.1/DBG_DAT | TP3/TP13| Programmer interface |
| 16| P2.2/DBG_CLK | TP5/TP15 | Programmer interface|
| 17| P2.3/XOSC32_Q1 | 32.768 kHz crystal |
| 18| P2.4/XOSC32_Q2 | 32.768 kHz crystal |
| 19| AVDD 
| 20| XOSC_Q2 | Crystal oscillator pin 2 |
| 21| XOSC_Q1 | Crystal oscillator pin 1, or external clock input |
| 22| AVDD | Power (Analog) 2.0 V - 3.6 V analog power supply connection |
| 23| RF_P | RF Positive RF input signal to LNA in receive mode |
| 24| RF_N | RF Negative RF input signal to LNA in receive mode |
| 25| AVDD | Analog Power 
| 26| AVDD | Analog Power 
| 27| RBIAS | Analog I/O External precision bias resistor for reference current |
| 28| GUARD | Power (Digital) Power supply connection for digital noise isolation |
| 29| AVDD_DREG |Power (Digital) 2.0 V - 3.6 V digital power supply for digital core voltage regulator |
| 30| DCOUPL | Power decoupling 1.8 V digital power supply decoupling |
| 31| RESET_N | TP2/TP12 |
| 32| P1.7/USART1 RXD | TP9/TP19 |
| 33| P1.6/USART1 TXD | TP7/TP17 |
| 34| P1.5 | EPD D1/SDIN | display pin 14 |
| 35| P1.4 | [See note 1](#notes) ||
| 36| P1.3 | EPD DO/SCK | display pin 13|


## Port 0 bits
| Bit | Direction | Connection | notes|
| -| -| -| -|
|P0.0 | output | EPD BS1 | display pin 8|
|P0.1 | TP6/TP16 ||
|P0.2 | EPD nCS1 [See note 3](#notes) |display pin 1 |
|P0.3 | output | SPI EEPROM CLK|
|P0.4 | output | SPI EEPROM MOSI|
|P0.5 | input | SPI EEPROM MISO|
|P0.6 | output | EPD nEnable| [See note 2](#notes)|
|P0.7 | output | EPD D/nC | display pin 11|

## Port 1 bits

| Bit | Direction | Connection | notes|
| -| -| -| -|
|P1.0 | EPD BUSY | display pin 9|
|P1.1 | EPD nCS | display pin 12|
|P1.2 | output | | EPD nRESET | display pin 10|
|P1.3 | output | EPD DO/SCK | display pin 13|
|P1.4 | [See note 1](#notes)||
|P1.5 | output | EPD D1/SDIN | display pin 14 |
|P1.6 | output | /USART1 TXD | TP7/TP17 |
|P1.7 | input | /USART1 RXD | TP9/TP19 |

## Port 2 bits
| Bit | Direction | Connection | notes|
| -| -| -| -|
|P2.0 | output | EEPROM nCS ||
|P2.1/DBG_DAT | TP3/TP13| Programmer interface |
|P2.2/DBG_CLK | TP5/TP15 | Programmer interface|
|P2.3/XOSC32_Q1 | 32.768 kHz crystal |
|P2.4/XOSC32_Q2 | 32.768 kHz crystal |

## Notes

1. P1.4 is connected to R3 on issue 8 Chroma 29s with a note in the silkscreen
which says "fit on Chroma".  The resistor was NOT populated. The other end of
R3 goes to EPD pin 2 GDR which is N-Channel FET gate drive control.  Since P1.4
is SPI0's MISO it is not possible to read data from the EPD on this board rev.
<br><br>
P1.4 appears to be N/C on issue 9 Chroma 29s and the silkscreen note "fit on
Chroma" is not present.

2. The display enable circuitry was not present on early hardware revisions.

3. P0.2 is EPD nCS1 on the Chroma 42, it appears to be N/C on Chroma 29.

