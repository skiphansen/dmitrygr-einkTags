## Reverse engineering EPDs

<a href="https://github.com/user-attachments/assets/bab9f8c8-bdba-4524-86ee-aad85661f514"> <img src="https://github.com/user-attachments/assets/bab9f8c8-bdba-4524-86ee-aad85661f514" width=50%>

This directory contains logic analyzer traces of the EPD's SPI bus
that I used for reverse engineering.

Software to view the traces on Linux, MacOS or Windoze can be downloaded from 
https://www.dreamsourcelab.com/download/ or it can be built from sources
https://github.com/DreamSourceLab/DSView.

Note: A logic analyzer is **NOT** necessary to view the traces.

## Files

| File | |
| -| -|
| Chroma21_epd_two_color.dsl| Chroma 21 EPD capture displaying image |
| Chroma29_epd_two_color.dsl| Chroma 29 EPD capture, part 1 |
| Chroma29_epd_end.dsl | Chroma 29 EPD capture, part  2|
|Chroma42_epd_two_color.dsl | Chroma 42 EPD capture, part 1 |
|Chroma42_epd_two_color_2.dsl | Chroma 42 EPD capture, part 2 |
|Chroma42_epd_two_color_3.dsl| Chroma 42 EPD capture, part 3 |
|Chroma42_epd_two_color_4.dsl| Chroma 42 EPD capture, part 4 |
|EPD_SPI.dsc | Configuation file for SPI capture of Chroma 29 |
|EPD_SPI_Chroma42.dsc | Configuation file for SPI capture of Chroma 42 |
|Solum_EL022F6W4A.dsl|  BWRY Solum EL022F6W4A startup<br>Displaying info screen!|
|cc1101.dsc | Configuation file for SPI capture of CC1101 connected to nrf52840 dongle |

## EPD SPI Sigrok decoder

When studying the data sent to the EPD display it is important to know if a
byte was sent as command or not.  I modified the spi decoder to include
the Data / Command bit with the SPI data to make ie easier to read.

For example this is decode of the startup of a Chroma29

<img width="1253" height="459" alt="sigrok_epd_decode" src="https://github.com/user-attachments/assets/4406d66b-b31b-4f2c-82cd-8d000afb765b" />

To install this decoder simply copy the spi_epd subdiretory to same subdirectory
where the rest of the DSView decoders are stored (/usr/local/share/libsigrokdecode4DSL/decoders on Linux).

## Dream Source Labs

Just as an aside, I know Dream Source Labs had a rocky startup with the 
[sigrok](http://sigrok.org/wiki/Main_Page) community, but I feel they are 
living up to the GPL.  

Dream Source has not only released their fork of Sigrok on github they are
actively maintaining it PUBLICALLY.  If fact the last commit was today.

I fully endorse their DSLogic Plus product, I think it's one of the best
purchases I've made.  Just beware there are DSLogic (without the plus) 
logic analyzers on ebay that lack the 256M bit buffer. The difference is
just a DRAM chip, but save yourself the hassle and make sure you get the
"plus" version. 

## 24 Pin EPD interface 

<img width="809" height="941" alt="image" src="https://github.com/user-attachments/assets/89e26b63-4538-417d-8302-59fda625f974" />

## Logic Analyzer Connections

| Pin | Signal | Note |
| - | - | - |
| 1 | Slave Chip Select | Rare, only if connected |
| 9 | Busy |
| 10 | Reset |
| 11 | D/C | Typical, only needed if pin 8 is grounded |
| 12 | Chip Select |
| 13 | Clock |
| 14 | Serial Data |



