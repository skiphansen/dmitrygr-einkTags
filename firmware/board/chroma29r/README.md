# Chroma 29 Info

EEPROM is a MX25V1006E (128k bytes)

## Building firmware

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

## Chroma 29 Test points

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

## CC Debugger connections

I hot glued a 5 pin SIP header to my board for connections to a programmer. 
The pinout matches a cable I already had for another project,

| SIP Pin | Test point | Signal | CC debugger pin |
|-|-|-|-|
|1|TP8 | J3, GND |  1 |
|2|TP5 | DC | 3 |
|3|TP4| J2, +VBAT | 2 (and 9 to power board from debugger) |
|4|TP3 | DD | 4 |
|5|TP2  | Reset_n | 7 |


## Debug serial port connections with FTDI cable

I hot glued a 3 pin SIP header to my board for connections to a programmer. 
The pinout matches a cable I already had for another project,

| SIP Pin | Test point | Signal | FTDI |
|-|-|-|-|
|1| J3  | GND | Black |
|2|TP7 | Serial out | Yellow |
|3|TP9 | Serial in | Orange |


