BUILD		?= chroma74y

#file containing main() must be first!
SOURCES	= main.c eeprom.c drawing.c ccm.c comms.c settings.c chars.c cpu.c 
SOURCES += board.c screen.c
include ../make/common.mk

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
FLAGS += -I. -MD

BUILD_DIR=$(BUILDS_DIR)/$(BUILD)
include $(FIRMWARE_ROOT)/make/rules.mk


