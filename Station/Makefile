CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY	= arm-none-eabi-objcopy
LKR		= nrf52.lkr
LTO		= -flto
COMMON	= -g -ggdb3 -Os -mthumb -march=armv7e-m  -fomit-frame-pointer -I. $(LTO) -munaligned-access -fno-strict-aliasing -Wall -Werror -fsection-anchors -fconserve-stack -Wno-error=unused-function
CCFLAGS	= $(COMMON) -ffunction-sections -fdata-sections -DNRF52840_XXAA
LDFLAGS	= $(COMMON) -Wl,--gc-sections -Wl,-T $(LKR)

APP = Station

OBJS += crt.o main.o system_nrf52840.o printf.o timebase.o radio.o ccm.o comms.o aes.o usb.o msc.o imgStore.o led.o tiRadio.o

all: $(APP).bin

test: $(APP).bin
	sudo CortexProg write $(APP).bin trace 0x2003fffc

$(APP).bin: $(APP).elf
	$(OBJCOPY) -I elf32-littlearm -O binary $(APP).elf $(APP).bin -j.text -j.data -j.rodata -j.vectors

$(APP).elf: $(OBJS) $(LKR)
	$(LD) -o $(APP).elf $(LDFLAGS) $(OBJS)

%.o : %.c Makefile
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(APP).bin $(APP).elf

