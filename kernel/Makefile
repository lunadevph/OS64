BUILD ?= ../build/obj/kernel
OUTPUT ?= $(BUILD)/kernel.bin
CONFIG_HEADER ?= ../build/generated/build_config.h
PACKAGE_HEADER ?= ../build/generated/package_catalog.h
.DEFAULT_GOAL := all
CC := gcc
LD := ld
NASM := nasm

__projroot := $(abspath $(CURDIR)/..)
__rel = $(patsubst $(__projroot)/%,%,$(1))
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-m64 -mno-red-zone -mgeneral-regs-only -Wall -Wextra -Werror -O2 \
	-Iinclude/os64 -I../sdk/include -I../third_party/bearssl/inc -I../third_party/bearssl/src
CFLAGS += -include $(CONFIG_HEADER) -include $(PACKAGE_HEADER)
LDFLAGS := -nostdlib -z noexecstack -z max-page-size=0x1000 -T arch/x86_64/linker.ld
C_SOURCES := core/kernel.c core/bootopts.c core/elf.c core/panic.c core/service.c core/timed.c \
	arch/x86_64/interrupts/exceptions.c drivers/acpi/acpi.c drivers/disk/disk.c \
	drivers/display/display.c drivers/display/ansi_terminal.c drivers/display/bochs_vbe.c drivers/display/graphics.c drivers/display/log.c drivers/keyboard/keyboard.c drivers/network/netdev.c drivers/network/pcnet.c drivers/network/rtl8139.c drivers/rtc/rtc.c \
	drivers/random/random.c \
	drivers/pci/pci.c \
	fs/fs.c fs/vfs.c fs/vfs_access.c fs/ext2.c fs/ext_variant.c fs/varfs.c fs/tmpfs.c fs/procfs.c net/net.c mm/memory.c security/auth.c security/crypto.c security/ofp.c \
	security/tls_platform.c security/tls_client.c security/ca_store.c security/libc_compat.c
C_OBJECTS := $(addprefix $(BUILD)/,$(notdir $(C_SOURCES:.c=.o)))
BEARSSL_SOURCES := $(shell find ../third_party/bearssl/src -type f -name '*.c' ! -name sysrng.c)
BEARSSL_OBJECTS := $(addprefix $(BUILD)/bearssl/,$(notdir $(BEARSSL_SOURCES:.c=.o)))
OBJECTS := $(BUILD)/boot.o $(BUILD)/exceptions_asm.o $(C_OBJECTS) $(BEARSSL_OBJECTS)
$(C_OBJECTS): $(CONFIG_HEADER) $(PACKAGE_HEADER)
vpath %.c $(sort $(dir $(BEARSSL_SOURCES)))

ifeq ($(V),1)
Q :=
else
Q := @
MAKEFLAGS += --no-print-directory
endif

.PHONY: all clean
all: $(OUTPUT)
$(BUILD):
	$(Q)printf "  %-7s %s\n" "MKDIR" "$(call __rel,$@)"
	$(Q)mkdir -p $@
$(BUILD)/boot.o: arch/x86_64/boot/boot.asm | $(BUILD)
	$(Q)printf "  %-7s %s\n" "AS" "$(call __rel,$@)"
	$(Q)$(NASM) -f elf64 $< -o $@
$(BUILD)/exceptions_asm.o: arch/x86_64/interrupts/exceptions.asm | $(BUILD)
	$(Q)printf "  %-7s %s\n" "AS" "$(call __rel,$@)"
	$(Q)$(NASM) -f elf64 $< -o $@
$(BUILD)/%.o: core/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: arch/x86_64/interrupts/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/acpi/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/disk/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/display/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/display.o: drivers/display/font8x16.h
$(BUILD)/%.o: drivers/keyboard/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/network/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/rtc/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/random/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: drivers/pci/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: net/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: fs/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: mm/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: security/%.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/bearssl/%.o: %.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)mkdir -p $(BUILD)/bearssl
	$(Q)$(CC) $(CFLAGS) -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -Wno-unused-parameter -Wno-sign-compare -c $< -o $@
$(OUTPUT): $(OBJECTS) arch/x86_64/linker.ld
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJECTS) -o $@
clean:
	$(Q)find $(BUILD) -depth -delete 2>/dev/null || true
