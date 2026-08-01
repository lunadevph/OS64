SDK ?= ../sdk
BUILD ?= build
OUTPUT ?= $(BUILD)/app
SOURCES ?= app.c
CC ?= gcc
LD ?= ld
LIBC ?= $(SDK)/../libc
LIBC_BUILD ?= $(BUILD)/libc
LIBC_ARCHIVE := $(LIBC_BUILD)/libos64c.a

__projroot := $(abspath $(CURDIR)/../../..)
__rel = $(patsubst $(__projroot)/%,%,$(1))
CFLAGS += -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -I$(SDK)/include -I$(LIBC)/include
LDFLAGS += -nostdlib -z noexecstack -T $(SDK)/linker.ld
OBJECTS := $(addprefix $(BUILD)/,$(SOURCES:.c=.o))

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean
all: $(OUTPUT)
$(BUILD):
	$(Q)printf "  %-7s %s\n" "MKDIR" "$(call __rel,$@)"
	$(Q)mkdir -p $@
$(BUILD)/%.o: %.c | $(BUILD)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(LIBC_ARCHIVE):
	$(Q)$(MAKE) -C $(LIBC) BUILD=$(abspath $(LIBC_BUILD)) OUTPUT=$(abspath $@)
$(OUTPUT): $(OBJECTS) $(LIBC_ARCHIVE)
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJECTS) $(LIBC_ARCHIVE) -o $@
clean:
	$(Q)find $(BUILD) -depth -delete 2>/dev/null || true
