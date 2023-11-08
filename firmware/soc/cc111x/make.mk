
CPU = 8051

FLAGS += --xram-loc 0xf000 --xram-size 0xda2 --model-medium
SOURCES += $(SOC_DIR)/wdt.c $(SOC_DIR)/timer.c $(SOC_DIR)/sleep.c 
SOURCES += $(SOC_DIR)/adc.c $(SOC_DIR)/radio.c $(SOC_DIR)/aes.c 
SOURCES += $(SOC_DIR)/soc.c $(SOC_DIR)//printf.c


test: main.bin
	sudo cc-tool -e -w $^ -f
