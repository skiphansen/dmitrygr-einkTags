
CPU = 8051

FLAGS += --xram-loc 0xf000 --xram-size 0xda2 --model-medium

test: main.bin
	sudo cc-tool -e -w $^ -f
