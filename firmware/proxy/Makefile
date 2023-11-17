BUILD ?= chroma29r

#file containing main() must be first!
SOURCES	= $(CURDIR)/main.c board.c eeprom.c $(COMMON_DIR)/SerialFraming.c
include ../make/common.mk

FLAGS += -DNO_AUTO_UART_SWITCH -DBUILD_TYPE="$(BUILD)" -MD
FLAGS += -I. -I$(FIRMWARE_ROOT)/rfcat/include

BUILD_DIR=$(BUILDS_DIR)/proxy_$(BUILD)
IMAGE_NAME=proxy_$(BUILD)

include $(FIRMWARE_ROOT)/make/rules.mk

