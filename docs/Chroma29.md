## Chroma29 Status

Dimitry's original code (firmware/main) is assumed to work on the tags he
has.  

However Dimitry's original code does not work on boards marked "edk286 Issue 8" 
or "edk286 Issue Issue 9". 

I have succeeded in reverse engineering these tags, but since I'm primarily 
interested in the OpenEPaperLink project I will not be adding support for
these tags to Dimitry's original code.

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/chroma29_working.jpg">

## History

The were several versions of the Chroma29 which are/were manufactured,
unfortunately they are not all compatible.  

I have identified two different versions in the batch of Chroma29s I have.

If you have version of the Chroma29r not listed before I would be interesting
in knowning any details you can provide.

I do not have any detailed information on the board version that Dmitry
based his firmware on.  

| SN Format | Board Rev | Controller | EPD Panel | Notes |
| :-: | - | - | - | - |
| ? | unknown | similar to UC8151C || Rev used by Dmitry's original firwmare |
| JAxxxxxxxxB | edk286 Issue 8<br>220-0067-08<br>2014| similar to UC8154 | WF0290T1PBZ01 | Display is always enabled (nEnable is ignored)|
| JAxxxxxxxxB | edk286 Issue 9<br>220-0067-09<br>2015| similar to UC8154 | WF0290T1PBZ01|1. EPD pin 4&5 (VGL,VGH) are n/c<br>2. Q2 & Q3 added for nEnable|
| JAxxxxxxxxB | [edk286 Issue 12](https://cdn.discordapp.com/attachments/1106088696584876052/1224138482306383923/IMG_7029.jpg?ex=661c66cd&is=6609f1cd&hm=7af524d4f4b003dd10a8b7ce95a92a0cf92a4bd327b9f30f46a64a0cccc190c5&)<br>220-0067-12<br>2015| ? | [WF0290T1DBZ010E4](https://cdn.discordapp.com/attachments/1106088696584876052/1224144421122216006/IMG_7034.jpg?ex=661c6c55&is=6609f755&hm=d90167ec9b87c30f2444976c0114b9a9c4dbe3621faec07862d7b0640a72ddb0&) | |
| ? | [edk296 Issue 1](https://user-images.githubusercontent.com/1102694/112720670-0b763400-8f00-11eb-93ba-e635734bff6d.jpg)<br>220-0094-01<br>2018 |?|WFD0290BF33|
| MExxxxxxxxC | [edk505 Issue4](https://cdn.discordapp.com/attachments/1134835190460583976/1220472054252568596/rn_image_picker_lib_temp_d4af2f14-4cf5-437b-97bc-031d243d11fb.jpg?ex=660f102d&is=65fc9b2d&hm=f0c94fa63e17d651fb66460a65a4276f1cfffa73e72e498f8f2392b036a9bf32&)<br>220-0095-004<br>2019| ? | DEPG0290RWU51011HPB1E|Processor is a CC1310 |
# General Chroma 29 Info

Resolution 296 x 128 BWR or BWY.

Known EPD controllers are either similar to the [UC8151](https://www.orientdisplay.com/wp-content/uploads/2022/09/UC8151C.pdf) or the [UC8154](https://v4.cecdn.yun300.cn/100001_1909185148/UC8154.pdf) depending on version.
                                                                                                
The SPI flash chip is the 128k byte [MX25V1006E](https://www.macronix.com/Lists/Datasheet/Attachments/8649/MX25V1006E,%202.5V,%201Mb,%20v1.5.pdf).

The flash is organized as 32 4k sectors or 2 65k blocks. Sectors, blocks, or 
the entire chip can be erased at a time.

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

## UC8154 LUTs

The following tables were captured from the EPD SPI bus while sending an 
image using [atc1441's Custom PriceTag Access Point](https://github.com/atc1441/E-Paper_Pricetags/tree/main/Custom_PriceTag_AccesPoint) to
a tag running stock firmware.

|Table|Values|
|:-:|-|
|Vcom1 LUT<br>(Cmd 0x20)|0x01 0x01 0x01 0x03 0x04 0x09 0x06 0x06<br>0x0A 0x04 0x04 0x19 0x03 0x04 0x09 |
|White LUT<br>(Cmd 0x21)|0x01 0x01 0x01 0x03 0x84 0x09 0x86 0x46<br>0x0A 0x84 0x44 0x19 0x03 0x44 0x09 |
|Black LUT<br>(Cmd 0x22)|0x01 0x01 0x01 0x43 0x04 0x09 0x86 0x46<br>0x0A 0x84 0x44 0x19 0x83 0x04 0x09 |
|Gray1 LUT<br>(Cmd 0x23)||
|Gray2 LUT<br>(Cmd 0x24)||
|Vcom2 LUT<br>(Cmd 0x25)|0x0A 0x0A 0x01 0x02 0x14 0x0D 0x14 0x14<br>0x01 0x00 0x00 0x00 0x00 0x00 0x00|
|Red0 LUT<br>(Cmd 0x26)|0x4A 0x4A 0x01 0x82 0x54 0x0D 0x54 0x54<br>0x01 0x00 0x00 0x00 0x00 0x00 0x00 |
|Red1 LUT<br>(Cmd 0x27)|0x0A 0x0A 0x01 0x02 0x14 0x0D 0x14 0x14<br>0x01 0x00 0x00 0x00 0x00 0x00 0x00|


