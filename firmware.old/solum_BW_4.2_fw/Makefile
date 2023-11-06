CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY	= arm-none-eabi-objcopy
LKR		= mz100.lkr
LTO		= -flto
COMMON	= -Os -mthumb -march=armv7-m  -fomit-frame-pointer -I. $(LTO) -munaligned-access
CCFLAGS	= $(COMMON) -ffunction-sections -fdata-sections
LDFLAGS	= $(COMMON) -Wl,--gc-sections -Wl,-T $(LKR)

APP			= PatchedFW
STOCK		= stockfw.bin
CHAR_IMGS	= chars.img

OBJS += crt.o main.o ccm.o timer.o chars.o settings.o display.o util.o comms.o heap.o

all: QSPI.bin

QSPI.bin: $(APP).1.bin $(APP).2.bin $(STOCK) $(CHAR_IMGS)
	cat $(STOCK) > $@
	truncate -s 53944 $@
	cat $(APP).1.bin >> $@
	truncate -s 61440 $@
	dd if=$(APP).2.bin of=$@ bs=1 seek=12432 conv=notrunc 2>/dev/null
	cat $(CHAR_IMGS) >> $@
	truncate -s 65536 $@
	/bin/echo -ne "\\x00\\xf0\\x00\\x00" | dd bs=1 conv=notrunc seek=8 of=$@ 2>/dev/null
	/bin/echo -ne "\\x00\\xbf\\x5f\\xf8\\x00\\xf0\\x99\\xd2\\x10\\x00" | dd bs=1 conv=notrunc seek=22706 of=$@ 2>/dev/null

$(APP).1.bin: $(APP).elf
	$(OBJCOPY) -I elf32-littlearm -O binary $< $@ -j.text -j.data -j.rodata -j.vectors -j.ver

$(APP).2.bin: $(APP).elf
	$(OBJCOPY) -I elf32-littlearm -O binary $< $@ -j.text2

$(APP).elf: $(OBJS) $(LKR)
	$(LD) -o $(APP).elf $(LDFLAGS) $(OBJS)

%.o : %.c Makefile
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(APP).1.bin $(APP).2.bin $(APP).elf

