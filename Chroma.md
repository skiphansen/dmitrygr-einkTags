# Displaydata Chroma (Aeon) Series Information

## Displaydata Chroma Aeon Series Specs

Three color, either black, white, and red (BWR) or black, white, and yellow (BWY).

| Model | Screen size | Resolution | EEPROM size | Supported |
| -| -| - | - | -|
| Chroma 16 | 1.08 x 1.08 in | 152 x 152 |? | N |
| Chroma 21 | 1.9 x 1.00  in | 212 x 104 |? | N |
| Chroma 27 | 2.4 x 1.20  in | 296 x 152 |? | N |
| Chroma 29 | 2.6 x 1.10  in | 296 x 128 | 128K | Y (some versions) |
| Chroma 37 | 3.2 x 1.85  in | 416 x 240 |? | N |
| Chroma 42 | 3.3 x 2.50  in | 400 x 300 | 1024K | N |
| Chroma 60 | 4.7 x 3.50  in | 648 x 480 |? | N |
| Chroma 74 | 6.4 x 3.50  in | 640 x 384 | 1024K | Y |
| Chroma 74H+ (?) | 6.4 x 3.90  in | 800 x 480 | ? | N |
| Chroma 125 | 10.0 x 7.50  in | 1304 x 984 | | N |

## Displaydata Aura Aeon Series Specs

Black and white only.

| Model | Screen size | Resolution | EEPROM size | Supported |
| -| -| - | - | -|
| Aura 21 | 1.9 x 1.00  in | 212 x 104 |? | N |
| Aura 29 | 2.6 x 1.10  in | 296 x 128 | ? | N |
| Aura 29F | 2.6 x 1.10  in | 296 x 128 | ? | N |
| Aura 42 | 3.3 x 2.50  in | 400 x 300 | ? | N |

## Frequencies used by Dmitry code

25 600 Khz spaced channels 903MHz to 927MHz
Channel 0: 902.999756
Channel 1: 903.599609
...
Channel 12
...
Channel 24


250 Kbps, 165KHz deviation, GFSK

Packets are variable length, CRC16 will be provided by the radio, as will whitening. 



## CC1110F32 Memory map

32k program FLASH + 4k SRAM

0xff00-0xffff   256 bytes fast access RAM
0xf000-0xfeff   4k - 256 bytes Slow access SRAM
0xdf80-0xdfff   Hardware SFR registers
0xdf00-0xdf7f   Hardware Radio & I2S registers
0x0000-0x7fff   Flash


## CC1110F pin connections
| Pin | Pin name | usage | Notes |
| - | - | -|-|
| 1| P1_2 Port 1.2 |NC ?|
| 2| DVDD ||
| 3| P1_1 | EPD BUSY | display pin 9|
| 4| P1_0 | TP6/TP16 ||
| 5| P0_0 | EPD BS1 | display pin 8|
| 6| P0_1 | EPD nCS | display pin 12|
| 7| P0_2 | EPD nReset |display pin 10|
| 8| P0_3 | SPI EEPROM CLK|
| 9| P0_4 | SPI EEPROM MOSI|
| 10| DVDD
| 11| P0_5 | SPI EEPROM MISO|
| 12| P0_6 | EPD nEnable|
| 13| P0_7 | EPD D/nC | display pin 11|
| 14| P2_0 | EEPROM nCS ||
| 15| P2_1/DBG_DAT | TP3/TP13| Programmer interface |
| 16| P2_2/DBG_CLK | TP5/TP15 | Programmer interface|
| 17| P2_3/XOSC32_Q1 | 32.768 kHz crystal |
| 18| P2_4/XOSC32_Q2 | 32.768 kHz crystal |
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
| 32| P1_7/USART1 RXD | TP9/TP19 |
| 33| P1_6/USART1 TXD | TP7/TP17 |
| 34| P1_5 | EPD D1/SDIN | display pin 14 |
| 35| P1_4 | NC ?
| 36| P1_3 | EPD DO/SCK | display pin 13|


## Port 0 bits
| Bit | Direction | Connection | notes|
| -| -| -| -|
|P0_0 | output | EPD BS1 | display pin 8|
|P0_1 | EPD nCS | display pin 12|
|P0_2 | EPD nReset | display pin 10|
|P0_3 | output | SPI EEPROM CLK|
|P0_4 | output | SPI EEPROM MOSI|
|P0_5 | output | SPI EEPROM MISO|
|P0_6 | output | EPD nEnable|
|P0_7 | output | EPD D/nC | display pin 11|

## Port 1 bits

| Bit | Direction | Connection | notes|
| -| -| -| -|
|P1_0 | TP6/TP16 ||
|P1_1 | EPD BUSY | display pin 9|
|P1_2 | output | Port 1.2 |NC ?|
|P1_3 | output | EPD DO/SCK | display pin 13|
|P1_4 | NC ?
|P1_5 | output | EPD D1/SDIN | display pin 14 |
|P1_6 | output | /USART1 TXD | TP7/TP17 |
|P1_7 | input | /USART1 RXD | TP9/TP19 |

## Port 2 bits
| Bit | Direction | Connection | notes|
| -| -| -| -|
|P2_0 | output | EEPROM nCS ||
|P2_1/DBG_DAT | TP3/TP13| Programmer interface |
|P2_2/DBG_CLK | TP5/TP15 | Programmer interface|
|P2_3/XOSC32_Q1 | 32.768 kHz crystal |
|P2_4/XOSC32_Q2 | 32.768 kHz crystal |

