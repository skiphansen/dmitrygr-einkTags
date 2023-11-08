COMMON_DIR=$(FIRMWARE_ROOT)/common
BOARD_DIR=$(FIRMWARE_ROOT)/board/$(BUILD)
SOC_DIR=$(FIRMWARE_ROOT)/soc/$(SOC)
CPU_DIR=$(FIRMWARE_ROOT)/cpu/$(CPU)
BUILDS_DIR=$(FIRMWARE_ROOT)/builds
BUILD_DIR?=$(BUILDS_DIR)/build
PREBUILT_DIR?=$(FIRMWARE_ROOT)/prebuilt
VPATH = $(COMMON_DIR) $(BOARD_DIR) $(SOC_DIR) $(CPU_DIR) $(BUILD_DIR)

FLAGS += -I$(BOARD_DIR)
FLAGS += -I$(SOC_DIR)
FLAGS += -I$(CPU_DIR)
FLAGS += -I$(COMMON_DIR)
FLAGS += -I.

all:	#make sure it is the first target

include $(BOARD_DIR)/make.mk
include $(SOC_DIR)/make.mk
include $(CPU_DIR)/make.mk

