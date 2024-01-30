BUILD ?= chroma29r

#file containing main() must be first!
SOURCES	= $(CURDIR)/main.c
include ../make/common.mk

SOURCES	+= board.c eeprom.c $(COMMON_DIR)/SerialFraming.c radio.c
SOURCES	+= $(CURDIR)/soc.c
SOURCES += $(SOC_DIR)/printf.c $(SOC_DIR)/soc.c $(SOC_DIR)/adc.c
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/sleep.c $(SOC_DIR)/timer.c

FLAGS += -DNO_AUTO_UART_SWITCH -DBUILD_TYPE="$(BUILD)" -MD -DPROXY_BUILD
FLAGS += -I. 
FLAGS += --debug
# FLAGS += -DBAUD_115200
BUILD_DIR=$(BUILDS_DIR)/proxy_$(BUILD)
IMAGE_NAME=proxy_$(BUILD)

include $(FIRMWARE_ROOT)/make/rules.mk

