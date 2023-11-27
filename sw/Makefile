# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

SRCS=$(wildcard *.cc)
HDRS=$(wildcard *.h)
BAUD=19200
AVR_TYPE=attiny3224
AVR_FREQ=3'333'333L  # 3.3MHz (for accurate baudrate timing)
PROGRAMMER_DEV=/dev/ttyUSB0
PROGRAMMER_TYPE=jtag2updi
TARGET_PREFIX=build/main
ATPACK_ARCHIVE=Atmel.ATtiny_DFP.2.0.368.atpack.tar.xz
ATPACK_DIR=build/atpack

CFLAGS=-g -DF_CPU=$(AVR_FREQ) -DNDEBUG -std=c++17 -Wall -Os -Werror -Wextra -B $(ATPACK_DIR)/gcc/dev/$(AVR_TYPE) -isystem $(ATPACK_DIR)/include
AVRDUDE_FLAGS=-p $(AVR_TYPE) -c$(PROGRAMMER_TYPE) -P$(PROGRAMMER_DEV) -b$(BAUD)

MEMORY_TYPES=calibration eeprom efuse flash fuse hfuse lfuse lock signature application apptable boot prodsig usersig

ROOT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all backup clean disassemble eeprom flash fuses hex program requisites

all: hex

clean:
	rm -rf build

requisites:
	sudo apt install avr-libc gcc-avr pkg-config avrdude libudev-dev build-essential srecord

# Downloaded from http://packs.download.atmel.com/
$(ATPACK_DIR):
	mkdir -p '$(ATPACK_DIR)'
	cd '$(ATPACK_DIR)' && tar xaf '$(ROOT_DIR)/$(ATPACK_ARCHIVE)'

build/%.o: %.cc $(HDRS) $(ATPACK_DIR)
	avr-g++ $(CFLAGS) -mmcu=$(AVR_TYPE) -Wa,-ahlmns=build/$*.lst -c -o $@ $<

$(TARGET_PREFIX).elf: $(patsubst %.cc,build/%.o,$(SRCS))
	avr-g++ $(CFLAGS) -mmcu=$(AVR_TYPE) -o $@ $^
	chmod --silent a-x $@

build/%.flash.hex: build/%.elf
	avr-objcopy -j .text -j .data -O ihex $< $@

build/%.eeprom.hex: build/%.elf
	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex $< $@

build/%.fuses.hex: build/%.elf
	avr-objcopy -j .fuse -O ihex $< $@ --change-section-lma .fuse=0

build/%.lfuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x00 0x01 -offset  0x00 -O $@ -Intel

build/%.hfuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x01 0x02 -offset -0x01 -O $@ -Intel

build/%.efuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x02 0x03 -offset -0x02 -O $@ -Intel

hex: $(TARGET_PREFIX).flash.hex
#hex: $(TARGET_PREFIX).eeprom.hex
#hex: $(TARGET_PREFIX).lfuse.hex
#hex: $(TARGET_PREFIX).hfuse.hex
#hex: $(TARGET_PREFIX).efuse.hex

disassemble: $(TARGET_PREFIX).elf
	#avr-objdump -s -j .fuse $<
	avr-objdump -s -h $<
	avr-objdump -C -d $< 2>&1

eeprom: $(TARGET_PREFIX).eeprom.hex
	avrdude $(AVRDUDE_FLAGS) -U eeprom:w:$<

fuses: $(TARGET_PREFIX).lfuse.hex $(TARGET_PREFIX).hfuse.hex $(TARGET_PREFIX).efuse.hex
	avrdude $(AVRDUDE_FLAGS) -U lfuse:w:$(TARGET_PREFIX).lfuse.hex
	avrdude $(AVRDUDE_FLAGS) -U hfuse:w:$(TARGET_PREFIX).hfuse.hex
	avrdude $(AVRDUDE_FLAGS) -U efuse:w:$(TARGET_PREFIX).efuse.hex

flash: $(TARGET_PREFIX).flash.hex
	avrdude $(AVRDUDE_FLAGS) -v -U flash:w:$<

program: flash # eeprom fuses

# Read all known memory types from controller and write it into a file.
# For available memory types: $(MEMORY_TYPES).
backup:
	@for memory in $(MEMORY_TYPES); do \
	    avrdude $(AVRDUDE_FLAGS) -U $$memory:r:./$(AVR_TYPE).$$memory.hex:i; \
	done
