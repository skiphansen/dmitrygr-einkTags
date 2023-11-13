# set BOOTLOADER_SIZE to match your bootloader
BOOTLOADER_SIZE = 0x1400
LDFLAGS_FLASH = --code-loc $(BOOTLOADER_SIZE)
USB_DEVICE_SERIAL_NUMBER="`./new_serial.py`"

CC=sdcc
CFLAGS=-Iinclude -DBUILD_VERSION=`./revision.sh` -MD
CFLAGSold=--no-pack-iram $(CF)
LFLAGS=--xram-loc 0xF000 

apps2531 = global.rel
apps1111 = cc1111rf.rel global.rel cc1111_aes.rel

libsspicom = cc1110_spicom.rel chipcon_usbdebug.rel chipcon_dma.rel

all: bins Aura21

# helper targets 
chipcon_usb.rel: chipcon_usb.c include/*.h
	#./new_serial.py
	$(CC) $(CFLAGS) $(DEFINES)  -DUSB_DEVICE_SERIAL_NUMBER=$(USB_DEVICE_SERIAL_NUMBER) -c chipcon_usb.c

%.rel: %.c include/*.h
	$(CC) $(CFLAGS) $(DEFINES) -c $<

# firmware build targets
Aura21:	clean Aura21.hex

Aura21.hex: DEFINES = -DAURA21 -DCC1111 -DSPI_COM
Aura21.hex: $(apps1111) $(libsspicom)
	@printf "\n\n==Aura21.hex building==\n"
	sdcc $(CFLAGS) $(LFLAGS) appFHSSNIC.c $(libsspicom) $(apps1111) $(DEFINES)
	packihx <appFHSSNIC.ihx >bins/Aura21.hex

bins:
	mkdir bins

clean:
	@printf "\n==== CLEANING STUFF UP ====\n"
	rm -f *.ihx *.rel *.hex *.sym *.asm *.lst *.lnk *.map *.mem *.rst
