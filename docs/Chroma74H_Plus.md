# Chroma 74 H+

The display is 6.4 x 3.90 inch BWR or BWY with a resolution 800 x 400.

The SPI flash chip is the 1024K byte [MX25V8006E](https://www.macronix.com/Lists/Datasheet/Attachments/8645/MX25V8006E,%202.5V,%208Mb,%20v1.7.pdf).

The flash is organized as 256 4k sectors or 16 65k blocks. Sectors, blocks, or 
the entire chip can be erased at a time.

Unlike the other Chroma tags this tag uses an ARM Cortex M3 based CC1310F64 
controller rather than the (much) older CC1110.

The FCC ID is VC7120-0226

## Board Revisions

| SN Rev<br>(Last character)|Board Rev | EPD Controller | EPD Panel | Notes |
| :-: | - | - | - | - |
| "C" | edk511 Issue 1<br>220-00000907-01<br>2021| ? | 60105-00372<br>V03 0129| 

