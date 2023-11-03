baud=19200
src=main
avrType=attiny3224
avrFreq=3333333  # 3.3MHz (for accurate baudrate timing)
programmerDev=/dev/ttyUSB003
programmerType=arduino
atpackArchive=Atmel.ATtiny_DFP.2.0.368.atpack.tar.xz
build=build/$(src)
atpackDir=build/atpack

cflags=-g -DF_CPU=$(avrFreq) -Wall -Os -Werror -Wextra -B $(atpackDir)/gcc/dev/$(avrType) -isystem $(atpackDir)/include
avrdudeflags=-p $(avrType) -c$(programmerType) -P$(programmerDev) -b$(baud) -v

memoryTypes=calibration eeprom efuse flash fuse hfuse lfuse lock signature application apptable boot prodsig usersig

rootDir := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))

.PHONY: all backup clean disassemble dumpelf eeprom elf flash fuses help hex program requisites

help:
	@echo 'backup       Read all known memory types from controller and write it into a file. Available memory types: $(memoryTypes)'
	@echo 'clean        Delete automatically created files.'
	@echo 'disassemble  Compile source code, then disassemble object file to mnemonics.'
	@echo 'dumpelf      Dump the contents of the .elf file. Useful for information purposes only.'
	@echo 'eeprom       Extract EEPROM data from .elf file and program the device with it.'
	@echo 'elf          Create $(build).elf'
	@echo 'flash        Program $(build).hex to controller flash memory.'
	@echo 'fuses        Extract FUSES data from .elf file and program the device with it.'
	@echo 'help         Show this text.'
	@echo 'hex          Create all hex files for flash, eeprom and fuses.'
	@echo 'program      Do all programming to controller.'
	@echo 'requisites   Install pre-requisite packages.'

all: hex

clean:
	rm -rf build

requisites:
	sudo apt install avr-libc gcc-avr pkg-config avrdude libudev-dev build-essential srecord

$(atpackDir):
	@# Downloaded from http://packs.download.atmel.com/
	mkdir -p '$(atpackDir)'
	cd '$(atpackDir)' && tar xaf '$(rootDir)/$(atpackArchive)'

build/%.o: %.cc $(atpackDir)
	avr-g++ $(cflags) -mmcu=$(avrType) -Wa,-ahlmns=$(build).lst -c -o $@ $<

build/%.elf: build/%.o
	avr-g++ $(cflags) -mmcu=$(avrType) -o $@ $<
	chmod --silent a-x $@

elf: $(build).elf

build/%.flash.hex: build/%.elf
	avr-objcopy -j .text -j .data -O ihex $< $@

build/%.eeprom.hex: build/%.elf
	avr-objcopy -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 -O ihex $< $@

build/%.fuses.hex: build/%.elf
	avr-objcopy -j .fuse -O ihex $< $@ --change-section-lma .fuse=0
	test -e $@

build/%.lfuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x00 0x01 -offset  0x00 -O $@ -Intel

build/%.hfuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x01 0x02 -offset -0x01 -O $@ -Intel

build/%.efuse.hex: build/%.fuses.hex
	srec_cat $< -Intel -crop 0x02 0x03 -offset -0x02 -O $@ -Intel

hex: $(build).flash.hex
#hex: $(build).eeprom.hex
#hex: $(build).lfuse.hex
#hex: $(build).hfuse.hex
#hex: $(build).efuse.hex

disassemble: elf
	#avr-objdump -s -j .fuse $(build).elf
	avr-objdump -s $(build).elf
	avr-objdump -C -d $(build).elf 2>&1

eeprom: $(build).eeprom.hex
	avrdude $(avrdudeflags) -U eeprom:w:$(build).eeprom.hex

fuses: $(build).lfuse.hex $(build).hfuse.hex $(build).efuse.hex
	avrdude $(avrdudeflags) -U lfuse:w:$(build).lfuse.hex
	avrdude $(avrdudeflags) -U hfuse:w:$(build).hfuse.hex
	avrdude $(avrdudeflags) -U efuse:w:$(build).efuse.hex

dumpelf: elf
	avr-objdump -s -h $(build).elf

flash: hex
	echo avrdude $(avrdudeflags) -v -U flash:w:$(build).flash.hex

program: flash # eeprom fuses

backup:
	@for memory in $(memoryTypes); do \
	    avrdude $(avrdudeflags) -U $$memory:r:./$(avrType).$$memory.hex:i; \
	done
