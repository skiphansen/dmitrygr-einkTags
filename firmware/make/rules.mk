BUILD_DIR?=$(BUILDS_DIR)/build

OBJS = $(patsubst %.c,$(BUILD_DIR)/%.$(OBJFILEEXT),$(notdir $(SOURCES)))

$(BUILD_DIR)/%.$(OBJFILEEXT): %.c
	$(CC) -c $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.ihx: $(OBJS)
	rm -f $(BUILD_DIR)/$(notdir $@)
	$(CC) $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.elf: $(OBJS)
	$(CC) $(FLAGS) -o $(BUILD_DIR)/$(notdir $@) $^

%.bin: %.ihx
	objcopy $^ $(BUILD_DIR)/$(IMAGE_NAME).bin -I ihex -O binary
	cp $(BUILD_DIR)/$(IMAGE_NAME).bin $(PREBUILT_DIR)
	@# Display sizes, we're critically short for code and RAM space !
	@grep '^Area' $(BUILD_DIR)/main.map | head -1
	@echo '--------------------------------        ----        ----        ------- ----- ------------'
	@grep = $(BUILD_DIR)/main.map | grep XDATA
	@grep = $(BUILD_DIR)/main.map | grep CODE
	@echo -n ".bin size: "
	@ls -l $(PREBUILT_DIR)/$(IMAGE_NAME).bin | cut -f5 -d' '

$(BUILD_DIR):
	if [ ! -e $(BUILD_DIR) ]; then mkdir -p $(BUILD_DIR); fi

$(PREBUILT_DIR):
	if [ ! -e $(PREBUILT_DIR) ]; then mkdir -p $(PREBUILT_DIR); fi

.PHONY: all clean veryclean flash reset


all: $(BUILD_DIR) $(PREBUILT_DIR) $(TARGETS)

clean:
	rm -rf $(BUILD_DIR)

veryclean: 
	rm -rf $(BUILDS_DIR)
	rm -rf $(PREBUILT_DIR)

flash:	$(BUILD_DIR)/$(IMAGE_NAME).bin $(OBJS)
	cc-tool -e -w $<

reset:	
	cc-tool --reset

debug:
	@echo "FIRMWARE_ROOT=$(FIRMWARE_ROOT)"
	@echo "SOC_DIR=$(SOC_DIR)"
	@echo "CPU_DIR=$(CPU_DIR)"
	@echo "BUILD=$(BUILD)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "SOURCES=$(SOURCES)"
	@echo "TARGETS=$(TARGETS)"


