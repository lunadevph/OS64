BUILD ?= ../build/obj/libc
.DEFAULT_GOAL := all
OUTPUT ?= $(BUILD)/libos64c.a
CC := gcc
AR := ar
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -Iinclude -I../sdk/include
SOURCES := src/string.c src/stdlib.c src/stdio.c src/api.c src/ctype.c
OBJECTS := $(addprefix $(BUILD)/,$(notdir $(SOURCES:.c=.o)))
OBJECTS += $(BUILD)/sdk_terminal.o $(BUILD)/sdk_services.o $(BUILD)/sdk_tui.o
$(OBJECTS): ../sdk/include/app_abi.h
.PHONY: all clean
all: $(OUTPUT)
$(BUILD):
	mkdir -p $@
$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sdk_terminal.o: ../sdk/lib/terminal.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sdk_services.o: ../sdk/lib/services.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/sdk_tui.o: ../sdk/lib/tui.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@
$(OUTPUT): $(OBJECTS)
	$(AR) rcs $@ $^
clean:
	find $(BUILD) -depth -delete 2>/dev/null || true
