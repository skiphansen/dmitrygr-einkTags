
CPU = 8051
FLAGS += -Isoc/cc111x

FLAGS += --xram-loc 0xf000 --xram-size 0xda2 --model-medium
SOURCES += soc/cc111x/wdt.c soc/cc111x/timer.c soc/cc111x/sleep.c soc/cc111x/adc.c soc/cc111x/radio.c soc/cc111x/aes.c soc/cc111x/soc.c soc/cc111x/printf.c



test: main.bin
	sudo cc-tool -e -w $^ -f
