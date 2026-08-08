CC ?= gcc
LD ?= ld
SDK ?= ../../../sdk
OUTPUT ?= edit
OBJDIR ?= ../../../build/obj/user/edit
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-m64 -mno-red-zone -mgeneral-regs-only -Os -Wall -Wextra -Werror -I$(SDK)/include
LDFLAGS := -nostdlib -z noexecstack -T $(SDK)/linker.ld
.PHONY: all clean
all: $(OUTPUT)
$(OBJDIR):
	mkdir -p $@
$(OBJDIR)/app.o: app.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OUTPUT): $(OBJDIR)/app.o
	$(LD) $(LDFLAGS) $< -o $@
clean:
	find $(OBJDIR) -depth -delete 2>/dev/null || true
