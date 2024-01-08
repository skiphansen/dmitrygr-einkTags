BUILD		?= chroma74y

#file containing main() must be first!
SOURCES	= main.c eeprom.c drawing.c ccm.c comms.c settings.c chars.c cpu.c 
SOURCES += board.c screen.c
include ../make/common.mk
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/timer.c $(SOC_DIR)/sleep.c 
SOURCES += $(SOC_DIR)/adc.c $(SOC_DIR)/radio.c $(SOC_DIR)/aes.c 
SOURCES += $(SOC_DIR)/soc.c $(SOC_DIR)/printf.c

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

ifeq ($(RANGE_TEST),tx)
  FLAGS += -DRANGE_TEST_TX
  IMAGE_NAME=range_test_tx_$(BUILD)
else ifeq ($(RANGE_TEST),rx)
  FLAGS += -DRANGE_TEST_RX
  IMAGE_NAME=range_test_rx_$(BUILD)
else
endif

EEPROMDRV ?= eeprom.c
SOURCES += $(EEPROMDRV)
FLAGS += -I. -MD

BUILD_DIR=$(BUILDS_DIR)/$(BUILD)
include $(FIRMWARE_ROOT)/make/rules.mk


