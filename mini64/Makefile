BUILD ?= ../build/obj/mini64
OUTPUT ?= $(BUILD)/mini64.bin
CC := gcc
LD := ld
NASM := nasm

CFLAGS := -std=gnu11 -ffreestanding -nostdlib -m32 -mno-mmx -mno-sse \
	-Wall -Wextra -Werror -O2 -I.
LDFLAGS := -nostdlib -m elf_i386 -z max-page-size=0x1000 -T linker.ld

OBJECTS := $(BUILD)/boot.o $(BUILD)/kernel.o

.PHONY: all clean
all: $(OUTPUT)

$(BUILD):
	mkdir -p $@

$(BUILD)/boot.o: boot.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/kernel.o: kernel.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUT): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) $(OBJECTS) -o $@

clean:
	rm -rf $(BUILD)
