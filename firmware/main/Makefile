BUILD		?= chroma74y

FIRMWARE_ROOT=$(abspath ..)
BUILD_DIR=$(BUILDS_DIR)/$(BUILD)

#file containing main() must be first!
SOURCES	= main.c
include $(FIRMWARE_ROOT)/make/common.mk

SOURCES	+= eeprom.c drawing.c ccm.c comms.c settings.c chars.c cpu.c board.c
SOURCES += screen.c

ifeq ($(BARCODE),barcode)
  FLAGS += -DBARCODE
  SOURCES += barcode.c
else ifeq ($(BARCODE),datamatrix)
 # FLAGS += -DDATAMATRIX
 # SOURCES += datamatrix.c
 #i might release this later, but for now, just use barcodes :P
  FLAGS += -DBARCODE
  SOURCES += barcode.c
else
  SOURCES += $(warning "no barcode type set")
endif

EEPROMDRV ?= eeprom.c
SOURCES += $(EEPROMDRV)
FLAGS += -I.

include $(FIRMWARE_ROOT)/make/rules.mk


