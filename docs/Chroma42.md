# Chroma 42

The Chroma 42 display was reverse engineered by capturing the EPD SPI bus while 
sending an image using [atc1441's Custom PriceTag Access Point](https://github.com/atc1441/E-Paper_Pricetags/tree/main/Custom_PriceTag_AccesPoint) 
to a tag running stock firmware.

The commands were found to match the UC8154 fairly closely.  

The spec sheet that I have does not list the 0xe0 Cascade Setting (CCSET) 
command, nor does it mention the ability to cascade two chips in the features 
section. However cascade mode is documented in the pin description section.

The setting of Panel Setting (PSR) is odd since it selects a resolution
of 128x296 rather than the expected 200x300.  The resolution setting (TRES)
command 0x61 however selects the expected resolution.

It is certainly possible that I have misidentified the controller, but
if so it's close enough for my purposes.

## General Chroma 42 Info

Display is 3.3 x 2.50 inch BWR or BWY with a resolution 400 x 300.

The display controller supports 2 bits of gray scale for the B&W pixels and
one bit for red (yellow) pixels. 

The SPI flash chip is the 1024K byte [MX25V8006E](https://www.macronix.com/Lists/Datasheet/Attachments/8645/MX25V8006E,%202.5V,%208Mb,%20v1.7.pdf).

The flash is organized as 256 4k sectors or 16 65k blocks. Sectors, blocks, or 
the entire chip can be erased at a time.

## Connections For Flashing And Debugging

| SIP Pin | Test point | Signal | CC debugger pin | Wire color|
|-|-|-|-|-|
|1|TP8 | J3, GND |  1 | Black |
|2|TP5 | DC | 3 | Brown |
|3|TP4| J2, +VBAT | 2 <br>(and 9 to power <br>board from debugger) |Red|
|4|TP3 | DD | 4 |Yellow|
|5|TP2  | Reset_n | 7 |Orange|

## Debug Serial port Connections

Connecting a serial port to the Chroma is very handy for development 
but it is not needed if you just want to flash custom firmware.

| SIP Pin | Test point | Signal | FTDI |
|-|-|-|-|
|1|J3  | GND | Black |
|2|TP7 | Serial out | Yellow |
|3|TP9 | Serial in | Orange |

## Test Points

| Test point | Signal | 
|:-:|-|
| TP2  | Reset_n |
|TP3 | DD |
|TP4| J2, +VBAT | 
|TP5 | DC | 
|TP6 | P0_1 |
|TP7 | P1.6 (Serial out) |
|TP8 | J3, GND |
|TP9 | P1_7 (Serial in) |
|TP10| J1.23 PREVGL ?|
|TP11 | J1.21 PREVGH ? |

## Logic Analyzer Connections for EPD Reverse Engineering

| Signal | EPD pin | CC1110 pin | Logic Analyzer pin |
| -|-| -| - |
| GND | 18 |  - | 1 |
| MOSI | 14 | 34  | 2 |
| CLK | 13 | 36  | 3 |
| nReset |10 | 1  | 4 |
| nCS | 12 | 3  | 5|
| Busy | 9 | 4  | 6|
| D/nC | 11 | 13  | 7|
| nEnable | x | 12 | 8|
| nCS1 | 1 | 7 | 9|

## Sending New Image to Display

|| Data | CS0 | CS1 |
|-| -| -| -|
|initialize master and slave|<init commands> |Y|Y|
|send B&W data to master |<0x10> <15,000 bytes/60,000> B/W pixels| Y | N |
|send B&W data to slave |<0x10> <15,000 bytes/60,000> B/W pixels| N | Y |
|send red data to master |<0x13> <7,500 bytes/60,000> Red pixels| Y | N |
|send red data to slave|<0x13> <7,500 bytes/60,000> Red pixels| N | Y |
|enable cascade clock|<0xe0><0x01>|Y | N |
|update display|<0x12>|Y | Y |
|wait until busy goes inactive ||x | x |
|reload the LUTs with all zeros|<0x20>...<0x22>,<0x24>|Y|Y|
|init for shutdown |PSR, TRES, PLL|Y|Y|
|wait for busy||x | x |
|shutdown power |VDCS, PWR|Y|Y|

## Board Revisions

| SN Rev<br>(Last character)|Board Rev | Controller | EPD Panel | Notes |
| :-: | - | - | - | - |
| "B" | edk288 Issue 2<br>220-0073-02<br>2014| Two UC8154s in cascade mode | WF0420T1PBF04 | 

