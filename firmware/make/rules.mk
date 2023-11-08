OBJS = $(patsubst %.c,$(BUILD_DIR)/%.$(OBJFILEEXT),$(notdir $(SOURCES)))

$(BUILD_DIR)/%.$(OBJFILEEXT): %.c
	$(CC) -c $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.ihx: $(OBJS)
	rm -f $(BUILD_DIR)/$(notdir $@)
	$(CC) $^ $(FLAGS) -o $(BUILD_DIR)/$(notdir $@)

$(BUILD_DIR)/main.elf: $(OBJS)
	$(CC) $(FLAGS) -o $(BUILD_DIR)/$(notdir $@) $^

%.bin: %.ihx
	objcopy $^ $(BUILD_DIR)/$(notdir $@) -I ihex -O binary
	cp $(BUILD_DIR)/$(notdir $@) $(PREBUILT_DIR)/$(BUILD).bin

all: $(BUILD_DIR) $(PREBUILT_DIR) $(TARGETS)

.PHONY: clean veryclean

$(BUILD_DIR):
	if [ ! -e $(BUILD_DIR) ]; then mkdir -p $(BUILD_DIR); fi

$(PREBUILT_DIR):
	if [ ! -e $(PREBUILT_DIR) ]; then mkdir -p $(PREBUILT_DIR); fi

clean:
	rm -rf $(BUILD_DIR)

veryclean: 
	rm -rf $(BUILDS_DIR)
	rm -rf $(PREBUILT_DIR)

debug:
	@echo "BOARD_DIR=$(BOARD_DIR)"
	@echo "SOC_DIR=$(SOC_DIR)"
	@echo "CPU_DIR=$(CPU_DIR)"
	@echo "BUILD=$(BUILD)"
	@echo "BUILD_DIR=$(BUILD_DIR)"
	@echo "SOURCES=$(SOURCES)"


