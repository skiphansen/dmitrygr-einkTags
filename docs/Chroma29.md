# Chroma 29 Info

Resolution 296 x 128 BWR or BWY.

Known EPD controllers are either similar to the [UC8151](https://www.orientdisplay.com/wp-content/uploads/2022/09/UC8151C.pdf) or the [UC8154](https://v4.cecdn.yun300.cn/100001_1909185148/UC8154.pdf) depending on version.
												
The SPI flash chip is the 128k byte [MX25V1006E](https://www.macronix.com/Lists/Datasheet/Attachments/8649/MX25V1006E,%202.5V,%201Mb,%20v1.5.pdf).

The flash is organized as 32 4k sectors or 2 65k blocks. Sectors, blocks, or 
the entire chip cand be erased at a time.

## Building the Firmware

1. Ensure SDCC version 4.0.7 is in the path by running '. ../sdcc/setup_sdcc.sh'
2. run "make BUILD=chroma29r"

For example:

````bash
skip@Dell-7040:~/dmitrygr-einkTags/firmware/main$ . ../../sdcc/setup_sdcc.sh
Added /home/skip/dmitrygr-einkTags/sdcc/sdcc-4.0.7/bin to PATH
skip@Dell-7040:~/dmitrygr-einkTags/firmware/main$ make BUILD=chroma29r
sdcc -c main.c --code-size 0x7f80 -Isoc/cc111x --xram-loc 0xf000 --xram-size 0xda2 --model-medium -Icpu/8051 -mmcs51 --std-c2x --opt-code-size --peep-file cpu/8051/peep.def --fomit-frame-pointer -Iboard/chroma29r -Isoc/cc111x -Icpu/8051 -DBARCODE -I. -o main.rel

(truncated...)

objcopy /home/skip/dmitrygr-einkTags/firmware/builds/chroma29r/main.ihx /home/skip/dmitrygr-einkTags/firmware/builds/chroma29r/main.bin -I ihex -O binary
cp /home/skip/dmitrygr-einkTags/firmware/builds/chroma29r/main.bin /home/skip/dmitrygr-einkTags/firmware/prebuilt/chroma29r.bin
skip@Dell-7040:~/dmitrygr-einkTags/firmware/main$
````

## Chroma 29 Connections For Flashing And Debugging

I hot glued a two SIP headers to my board for connections to a programmer
and a serial port.  The pinouts were chosen to match cables I already had from 
another project.

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/chroma29_connections.png" width=50%>

## CC Debugger Connections

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


## Chroma 29 Test Points

| Test point | Signal | 
|-|-|
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

## History

The were several versions of the the Chroma29 which are/were manufactured,
unfortunately they are not all compatible.  The version of the Chroma29
that Dmitry developed his code for apparently used a different EPD panel
and hench EPD controller that the Chroma29s I bought on ebay.  I do not
have any detailed information on the version Dmitry had.

In the batch of Chroma29s I bought I have identified two different versions
so far.

| Rev | EPD Panel | Controller | Notes |
| - | - | - | - |
| unknown | similar to UC8151C || Rev used by Dmitry's original firwmare |
| edk286 Issue 8<br>220-0067-08<br>2014| similar to UC8154 | WF0290T1PBZ01 ||
| edk286 Issue 9<br>220-0067-09<br>2015| similar to UC8154 | WF0290T1PBZ01|1. EPD pin 4&5 (VGL,VGH) are n/c<br>2. Q2 & Q3 added<br>|


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

