# set defaults for BUILD_DIR and PREBUILT_DIR if not set already
FIRMWARE_ROOT:=$(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

FW_COMMON_DIR=$(FIRMWARE_ROOT)/common
COMMON_DIR=$(FIRMWARE_ROOT)/../common
BOARD_DIR=$(FIRMWARE_ROOT)/board/$(BUILD)
SOC_DIR=$(FIRMWARE_ROOT)/soc/$(SOC)
CPU_DIR=$(FIRMWARE_ROOT)/cpu/$(CPU)
BUILDS_DIR=$(FIRMWARE_ROOT)/builds
PREBUILT_DIR?=$(FIRMWARE_ROOT)/prebuilt
IMAGE_NAME?=$(BUILD)
VPATH = $(COMMON_DIR) $(FW_COMMON_DIR) $(BOARD_DIR) $(SOC_DIR) $(CPU_DIR) $(BUILD_DIR) 

FLAGS += -I$(BOARD_DIR)
FLAGS += -I$(SOC_DIR)
FLAGS += -I$(CPU_DIR)
FLAGS += -I$(COMMON_DIR)
FLAGS += -I$(FW_COMMON_DIR)
FLAGS += -I.

all:	#make sure it is the first target

include $(BOARD_DIR)/make.mk
include $(SOC_DIR)/make.mk
include $(CPU_DIR)/make.mk

