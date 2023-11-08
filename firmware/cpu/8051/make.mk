FLAGS += -I$(FIRMWARE_ROOT)/cpu/8051

FLAGS += -mmcs51 --std-c2x --opt-code-size --peep-file $(CPU_DIR)/peep.def --fomit-frame-pointer
SOURCES += $(CPU_DIR)/asmUtil.c
CC = sdcc

TARGETS	= main.ihx main.bin
OBJFILEEXT = rel


