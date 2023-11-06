SDCC		?=	~/sdcc-4.0.0/bin/sdcc
SDCC		= sdcc
APP			= EepromDump

FLAGS		+= -mmcs51 --std-c2x --xram-loc 0xf000 --xram-size 0xda2
FLAGS		+= --opt-code-speed --model-medium --peep-file peep.def --fomit-frame-pointer

BUILD		?= chroma74

#file containing main() must be first!
SOURCES		+= main.c timer.c printf.c eeprom.c adc.c asmUtil.c
SOURCES		+= sleep.c ccm.c radio.c comms.c settings.c chars.c

ifeq ($(BUILD),chroma74)
  SOURCES += screenEink74.c u1shared.c
  FLAGS += -DCHROMA74
  FLAGS += --code-size 0x8000
else ifeq ($(BUILD),chroma29)
  SOURCES += screenEink29.c u1shared.c
  FLAGS += -DCHROMA29
  FLAGS += --code-size 0x7f80
else ifeq ($(BUILD),epop50)
  SOURCES += screenLcd.c u1shared.c
  FLAGS += -DEPOP50
  FLAGS += --code-size 0x7f80
# 0x7F80 and not 0x8000 to leave some space for epop update "eeprom img download status" header to fit
else ifeq ($(BUILD),epop900)
  SOURCES += screenRaw.c u1private.c
  FLAGS += -DEPOP900
  FLAGS += --code-size 0x8000
else
  SOURCES += $(error "no build type set")
endif


#do not touch below


OBJS		= $(patsubst %.c,%.rel,$(SOURCES))

all: main.ihx main.bin

test: main.bin
	sudo cc-tool -e -w $^ -f

%.bin: %.ihx
	objcopy $^ $@ -I ihex -O binary

main.ihx: $(OBJS)
	rm -f $@
	$(SDCC) $^ $(FLAGS)

%.rel: %.c
	$(SDCC) -c $^ $(FLAGS)

.PHONY: clean
clean:
	rm -f $(OBJS) main.ihx main.bin main.lk
	rm -f $(patsubst %.c,%.asm,$(SOURCES)) $(patsubst %.c,%.lst,$(SOURCES)) $(patsubst %.c,%.rst,$(SOURCES)) $(patsubst %.c,%.sym,$(SOURCES))
	rm -f $(patsubst %.c,%.map,$(SOURCES)) $(patsubst %.c,%.mem,$(SOURCES)) $(patsubst %.c,%.ihx,$(SOURCES)) $(patsubst %.c,%.adb,$(SOURCES))
	rm -f $(patsubst %.c,%.cdb,$(SOURCES)) $(patsubst %.c,%.omf,$(SOURCES))

