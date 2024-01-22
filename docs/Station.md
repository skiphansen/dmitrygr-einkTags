## Modifications to Dmitry's code

Dmitry's orignal station code was setup to use his [CortexProg](https://cortexprog.com/) tool
for both flashing and debugging.  Unfortunately the project's kickstarter
was unsuccessful and I couldn't find a Cortexprog available for purchase 
anywhere.  That's too bad as the CortexProg looks like a very worthwhile tool!

To get around the unavailability of a CortexProg I added support for a debug 
serial port and flashing using a [Segger J-Link debugger](https://www.segger.com/products/debug-probes/j-link/).

## Building

The makefile assumes arm-none-eabi-gcc is in the path.  I have used the version 
7.2_2017q4 for arm successfully. 

````
skip@Dell-7040:~/esl/dmitrygr-einkTags/Station$ make
mkdir build
arm-none-eabi-gcc -c crt.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/crt.o
arm-none-eabi-gcc -c main.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/main.o
arm-none-eabi-gcc -c system_nrf52840.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/system_nrf52840.o
arm-none-eabi-gcc -c printf.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/printf.o
arm-none-eabi-gcc -c timebase.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/timebase.o
arm-none-eabi-gcc -c radio.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/radio.o
arm-none-eabi-gcc -c ccm.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/ccm.o
arm-none-eabi-gcc -c comms.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/comms.o
arm-none-eabi-gcc -c aes.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/aes.o
arm-none-eabi-gcc -c usb.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/usb.o
arm-none-eabi-gcc -c msc.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/msc.o
arm-none-eabi-gcc -c imgStore.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/imgStore.o
arm-none-eabi-gcc -c led.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/led.o
arm-none-eabi-gcc -c tiRadio.c -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD -DCONFIG_NFCT_PINS_AS_GPIOS -Os -o build/tiRadio.o
arm-none-eabi-gcc -o build/Station.elf -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -Wl,--gc-sections -Wl,-T nrf52.lkr build/crt.o build/main.o build/system_nrf52840.o build/printf.o build/timebase.o build/radio.o build/ccm.o build/comms.o build/aes.o build/usb.o build/msc.o build/imgStore.o build/led.o build/tiRadio.o
arm-none-eabi-objcopy -I elf32-littlearm -O binary build/Station.elf build/Station.bin -j.text -j.data -j.rodata -j.vectors
skip@Dell-7040:~/esl/dmitrygr-einkTags/Station$
````

## Flashing

I happened to have a Segger J-Link debugger which I use for flashing and
debugging my board, but it should be easy enough to add support for other 
programmers as well.

The nRF52840 dongle includes a footprint for an device programmer cable 
connector, but unfortunately it is not populated.  Since SWD only uses 4 wires
it's not too hard to tack solder the needed wires.

There's also a footprint for a [Tag-Connect](https://www.digikey.com/en/products/detail/tag-connect-llc/TC2050-IDC/2605366?utm_adgroup=&utm_source=google&utm_medium=cpc&utm_campaign=PMax%20Shopping_Product_Low%20ROAS%20Categories&utm_term=&utm_content=&utm_id=go_cmp-20243063506_adg-_ad-__dev-c_ext-_prd-2605366_sig-CjwKCAiAmsurBhBvEiwA6e-WPNWAEUe767qzWz7LBTrVi5Yp0nkL3_ebQTLieOhB_ZLUX4Is6SrZzRoCg68QAvD_BwE&gad_source=1&gclid=CjwKCAiAmsurBhBvEiwA6e-WPNWAEUe767qzWz7LBTrVi5Yp0nkL3_ebQTLieOhB_ZLUX4Is6SrZzRoCg68QAvD_BwE) plug of nails
connector.  I wish I had one but I don't and they are **EXPENSIVE** !

````
skip@Dell-7040:~/esl/dmitrygr-einkTags/Station$ make flash
arm-none-eabi-gcc -o build/Station.elf -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. -flto -munaligned-access -fno-strict-aliasing -Wall -Werror  -fsection-anchors -fconserve-stack -Wno-error=unused-function -Wl,--gc-sections -Wl,-T nrf52.lkr build/crt.o build/main.o build/system_nrf52840.o build/printf.o build/timebase.o build/radio.o build/ccm.o build/comms.o build/aes.o build/usb.o build/msc.o build/imgStore.o build/led.o build/tiRadio.o
arm-none-eabi-objcopy -I elf32-littlearm -O ihex build/Station.elf build/Station.hex -j.text -j.data -j.rodata -j.vectors
nrfjprog -f nrf52 --recover
Recovering device. This operation might take 30s.
Writing image to disable ap protect.
Erasing user code and UICR flash areas.
nrfjprog -f nrf52 --program build/Station.bin --sectorerase
Parsing image file.
Erasing page at address 0x0.
Erasing page at address 0x1000.
Erasing page at address 0x2000.
Erasing page at address 0x3000.
Applying system reset.
Checking that the area to write is not protected.
Programming device.
nrfjprog -f nrf52 -r
Applying system reset.
Run.
skip@Dell-7040:~/esl/dmitrygr-einkTags/Station$
````

## Logging output

The debug port is configured for 115200 baud, 8 data bits and no parity.

I use an FTDI [TTL-232R-3V3](https://www.digikey.com/product-detail/en/ftdi-future-technology-devices-international-ltd/TTL-232R-3V3/768-1015-ND/1836393) 
USB to 3.3 volt serial cable and minicom for logging.

With any luck you should see something similar to the following after successfully 
flashing the firmware

````
in entry
setting vregouin entry
usb init
enabling ints
setting allowed ints
pullup off
pullup on
Decided on a MAC 44:2E:47:2E:38:CD:AC:2C
Root key is 0x93FA5C8FA529B525920FF28B082ECAC1
Sub-GHz radio inited
rx is on
````

## nRF52840 to CC1101 connections

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/nRF52840-CC1101-connections.png" width=50%><br>
(Click picture to enlarge)

|nRF port | Signal | Usage | Direction |
|-|-|-|-|
0.15 | CS | chip select | nRF52840 -> CC1101 |
0.17 | GDO0 | read FIFO ready| CC1101 -> nRF52840 |
0.20 | GDO2 | write FIFO ready| CC1101 -> nRF52840 |
0.22 | MISO aka SO | SPI data from CC1101 | CC1101 -> nRF52840 |
0.24 | SCK  | SPI clock | nRF52840 -> CC1101 |
1.00 | MOSI aka SI | SPI data to CC1101 | nRF52840 -> CC1101 |
0.09 | RXD | Orange on a FTDI cable | PC -> nRF52840 (not needed) |
0.10 | TXD | Yellow on a FTDI cable | nRF52840 -> PC |
Ground | | Black on a FTDI cable||

## Example CC1101 module connections

These are the connections of the generic CC1101 module I sourced from ebay.

Do **NOT** assume the pinout of **your** module matches this one, **CHECK** it before wiring.

<img src="https://github.com/skiphansen/dmitrygr-einkTags/blob/master/assets/ebay_cc1101_pinout.png" width=50%><br>
(Click picture to enlarge)

