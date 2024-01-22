# Chroma 29 Info

Resolution 296 x 128 BWR or BWY.

EPD controller either [UC8151](https://www.orientdisplay.com/wp-content/uploads/2022/09/UC8151C.pdf) or [UC8154](https://v4.cecdn.yun300.cn/100001_1909185148/UC8154.pdf) depending on version.
												
SPI flash chip is a [MX25V1006E](https://www.macronix.com/Lists/Datasheet/Attachments/8649/MX25V1006E,%202.5V,%201Mb,%20v1.5.pdf) (128k bytes)

The chip is organized as 32 4k sectors or 2 65k blocks.

Sectors, blocks, or the entire chip cand be erased at a time.

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

## Connections to a CC Debugger

I hot glued a 5 pin SIP header to my board for connections to a programmer. 

The pinout matches a cable I already had for another project,

| SIP Pin | Test point | Signal | CC debugger pin | Wire color|
|-|-|-|-|-|
|1|TP8 | J3, GND |  1 | Black |
|2|TP5 | DC | 3 | Brown |
|3|TP4| J2, +VBAT | 2 <br>(and 9 to power <br>board from debugger) |Red|
|4|TP3 | DD | 4 |Yellow|
|5|TP2  | Reset_n | 7 |Orange|


## Connections For Debug Serial Port Using FTDI Cable

I hot glued a 3 pin SIP header to my board for connections to a programmer. 

The pinout was chosen to matche a cable I already had for another project.

| SIP Pin | Test point | Signal | FTDI |
|-|-|-|-|
|1| J3  | GND | Black |
|2|TP7 | Serial out | Yellow |
|3|TP9 | Serial in | Orange |

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/chroma29_connections.png" width=50%>

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

