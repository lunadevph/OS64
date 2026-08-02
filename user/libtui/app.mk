CC ?= gcc
LD ?= ld
SDK ?= ../../../sdk
LIBTUI ?= ../../libtui
OUTPUT ?= app
OBJDIR ?= ../../../build/obj/user/tui-app

__projroot := $(abspath $(CURDIR)/../../..)
__rel = $(patsubst $(__projroot)/%,%,$(1))
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -I$(SDK)/include -I$(LIBTUI)
ifneq ($(strip $(CONFIG_HEADER)),)
CFLAGS += -include $(CONFIG_HEADER)
endif
LDFLAGS := -nostdlib -z noexecstack -T $(SDK)/linker.ld
LIBSRC := $(LIBTUI)/screen.c $(LIBTUI)/theme.c $(LIBTUI)/dialog.c $(LIBTUI)/widgets.c
SOURCES := app.c $(LIBSRC)
OBJECTS := $(addprefix $(OBJDIR)/,$(notdir $(SOURCES:.c=.o)))

ifeq ($(V),1)
Q :=
else
Q := @
endif

.PHONY: all clean
all: $(OUTPUT)
$(OBJDIR):
	$(Q)printf "  %-7s %s\n" "MKDIR" "$(call __rel,$@)"
	$(Q)mkdir -p $@
$(OBJDIR)/app.o: app.c | $(OBJDIR)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(LIBTUI)/%.c | $(OBJDIR)
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$@)"
	$(Q)$(CC) $(CFLAGS) -c $< -o $@
$(OUTPUT): $(OBJECTS)
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $^ -o $@
clean:
	$(Q)find $(OBJDIR) -depth -delete 2>/dev/null || true
