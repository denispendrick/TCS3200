# Build the TCS3200 pyrometer firmware with the Microchip XC16 compiler.
#
#   make            # build build/tcs3200_pyrometer.hex
#   make clean
#
# Requires xc16-gcc / xc16-bin2hex on PATH (MPLAB XC16 install).
# Change DEVICE to match your dsPIC33.

DEVICE   = 33EP128GP502
MCU      = -mcpu=$(DEVICE)

CC       = xc16-gcc
BIN2HEX  = xc16-bin2hex

BUILD    = build
TARGET   = $(BUILD)/tcs3200_pyrometer

SRC      = $(wildcard src/*.c)
OBJ      = $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))

CFLAGS   = $(MCU) -O1 -Wall -Wextra -Isrc

all: $(TARGET).hex

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(MCU) -o $@ $(OBJ) -Wl,--report-mem

$(TARGET).hex: $(TARGET).elf
	$(BIN2HEX) $<

clean:
	rm -rf $(BUILD)

.PHONY: all clean
