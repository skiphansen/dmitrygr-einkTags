This directory contains logic analyzer traces of the EPD's SPI bus
that I used for reverse engineering.

Software to view the traces on Linux, MacOS or Windoze can be downloaded from 
https://www.dreamsourcelab.com/download/ or it can be built from sources
https://github.com/DreamSourceLab/DSView.

Note: A logic analyzer is **NOT** necessary to view the traces.

## Files

| File | |
| -| -|
| Chroma29_epd_two_color.dsl| Chroma 29 test, part 1 |
| Chroma29_epd_end.dsl | Chroma 29 test, part  2|
|Chroma42_epd_two_color.dsl | Chroma 42 test, part 1 |
|Chroma42_epd_two_color_2.dsl | Chroma 42 test, part 2 |
|Chroma42_epd_two_color_3.dsl| Chroma 42 test, part 3 |
|Chroma42_epd_two_color_4.dsl| Chroma 42 test, part 4 |
|EPD_SPI.dsc | Configuation file for SPI capture of Chroma 29 |
|EPD_SPI_Chroma42.dsc | Configuation file for SPI capture of Chroma 42 |
| cc1101.dsc | Configuation file for SPI capture of CC1101 connected to nrf52840 dongle |

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

