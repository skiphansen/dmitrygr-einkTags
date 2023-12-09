CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY	= arm-none-eabi-objcopy
LKR		= nrf52.lkr
LTO		= -flto
COMMON = -g -ggdb3 -mthumb -march=armv7e-m  -fomit-frame-pointer -I. $(LTO)
COMMON += -munaligned-access -fno-strict-aliasing -Wall -Werror 
COMMON += -fsection-anchors -fconserve-stack -Wno-error=unused-function

CCFLAGS	= $(COMMON) -ffunction-sections -fdata-sections -DNRF52840_XXAA -MD
CCFLAGS += -DCONFIG_NFCT_PINS_AS_GPIOS

LDFLAGS	= $(COMMON) -Wl,--gc-sections -Wl,-T $(LKR)

ifeq ($(DEBUG),y)
CCFLAGS	+= -O0
else
CCFLAGS	+= -Os
endif

BUILD_DIR = build

.PHONEY: all test flash clean reset

APP = Station

SOURCES = crt.c main.c system_nrf52840.c printf.c timebase.c radio.c ccm.c 
SOURCES += comms.c aes.c usb.c msc.c imgStore.c led.c tiRadio.c
VPATH = $(abspath .)

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))

all: $(APP).bin

test: $(APP).bin
	sudo CortexProg write $(APP).bin trace 0x2003fffc

flash: $(APP).hex
	nrfjprog -f nrf52 --recover
	nrfjprog -f nrf52 --program $(BUILD_DIR)/$(APP).bin --sectorerase
	nrfjprog -f nrf52 -r

reset:
	nrfjprog -f nrf52 -r

$(APP).bin: $(BUILD_DIR) $(APP).elf
	$(OBJCOPY) -I elf32-littlearm -O binary $(BUILD_DIR)/$(APP).elf $(BUILD_DIR)/$(APP).bin -j.text -j.data -j.rodata -j.vectors

$(APP).hex: $(BUILD_DIR) $(APP).elf
	$(OBJCOPY) -I elf32-littlearm -O ihex $(BUILD_DIR)/$(APP).elf $(BUILD_DIR)/$(APP).hex -j.text -j.data -j.rodata -j.vectors

$(BUILD_DIR):
	mkdir $(BUILD_DIR)

$(APP).elf: $(OBJS) $(LKR)
	$(LD) -o $(BUILD_DIR)/$(APP).elf $(LDFLAGS) $(OBJS)

$(BUILD_DIR)/%.o : %.c
	$(CC) -c $^ $(CCFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

debug:
	echo "OBJS= $(OBJS)"
	echo "VPATH= $(VPATH)"
