ROOTFS ?= ../build/rootfs
OBJ ?= ../build/obj/user
SDK := ../sdk

__projroot := $(abspath $(CURDIR)/..)
__rel = $(patsubst $(__projroot)/%,%,$(1))
CC := gcc
LD := ld
CFLAGS := -std=gnu11 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
	-m64 -mno-red-zone -Os -Wall -Wextra -Werror -I$(SDK)/include
LDFLAGS := -nostdlib -z noexecstack -T $(SDK)/linker.ld
BIN := $(shell sed -n 's/^bin=//p' commands.conf)
SBIN := $(shell sed -n 's/^sbin=//p' commands.conf)
BIN_TARGETS := $(addprefix $(ROOTFS)/bin/,$(BIN))
SBIN_TARGETS := $(addprefix $(ROOTFS)/sbin/,$(SBIN))
GAMES := $(notdir $(wildcard games/*))
GAME_TARGETS := $(addprefix $(ROOTFS)/usr/bin/,$(GAMES))
APPS := $(notdir $(wildcard apps/*))
APP_TARGETS := $(addprefix $(ROOTFS)/usr/bin/,$(APPS))
PACKAGE_SOURCE ?= ../packages/apps
PACKAGE_MANIFEST ?= ../packages/packages.json
PACKAGES := $(notdir $(wildcard $(PACKAGE_SOURCE)/*))
PACKAGE_DIR := $(ROOTFS)/usr/share/os64/packages
PACKAGE_TARGETS := $(addprefix $(PACKAGE_DIR)/,$(PACKAGES))
PACKAGE_CATALOG_TARGET := $(PACKAGE_DIR)/packages.json

ifeq ($(V),1)
Q :=
else
Q := @
MAKEFLAGS += --no-print-directory
endif

.PHONY: all clean $(GAME_TARGETS) $(APP_TARGETS) $(PACKAGE_TARGETS)
all: $(BIN_TARGETS) $(SBIN_TARGETS) $(GAME_TARGETS) $(APP_TARGETS) $(PACKAGE_TARGETS) $(PACKAGE_CATALOG_TARGET)
$(OBJ) $(ROOTFS)/bin $(ROOTFS)/sbin $(ROOTFS)/usr/bin $(PACKAGE_DIR):
	$(Q)printf "  %-7s %s\n" "MKDIR" "$(call __rel,$@)"
	$(Q)mkdir -p $@
$(ROOTFS)/bin/%: lib/builtin.c $(SDK)/include/app_abi.h $(SDK)/linker.ld | $(OBJ) $(ROOTFS)/bin
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$(OBJ)/$*.o)"
	$(Q)$(CC) $(CFLAGS) -DAPP_NAME='"$*"' -c $< -o $(OBJ)/$*.o
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJ)/$*.o -o $@
$(ROOTFS)/sbin/%: lib/builtin.c $(SDK)/include/app_abi.h $(SDK)/linker.ld | $(OBJ) $(ROOTFS)/sbin
	$(Q)printf "  %-7s %s\n" "CC" "$(call __rel,$(OBJ)/$*.o)"
	$(Q)$(CC) $(CFLAGS) -DAPP_NAME='"$*"' -c $< -o $(OBJ)/$*.o
	$(Q)printf "  %-7s %s\n" "LD" "$(call __rel,$@)"
	$(Q)$(LD) $(LDFLAGS) $(OBJ)/$*.o -o $@
define GAME_RULE =
$(ROOTFS)/usr/bin/$(1): | $(OBJ) $(ROOTFS)/usr/bin
	$$(Q)printf "  %-7s %s\n" "MAKE" "$$(call __rel,$$@)"
	$$(Q)$$(MAKE) -C games/$(1) OUTPUT=$$(abspath $$@) OBJDIR=$$(abspath $$(OBJ))/$(1) V=$$(V)
endef
$(foreach g,$(GAMES),$(eval $(call GAME_RULE,$(g))))
define APP_RULE =
$(ROOTFS)/usr/bin/$(1): | $(OBJ) $(ROOTFS)/usr/bin
	$$(Q)printf "  %-7s %s\n" "MAKE" "$$(call __rel,$$@)"
	$$(Q)$$(MAKE) -C apps/$(1) OUTPUT=$$(abspath $$@) OBJDIR=$$(abspath $$(OBJ))/$(1) CONFIG_HEADER=$$(CONFIG_HEADER) V=$$(V)
endef
$(foreach a,$(APPS),$(eval $(call APP_RULE,$(a))))
define PACKAGE_RULE =
$(PACKAGE_DIR)/$(1): | $(OBJ) $(PACKAGE_DIR)
	$$(Q)printf "  %-7s %s\n" "PACKAGE" "$$(call __rel,$$@)"
	$$(Q)$$(MAKE) -C $$(PACKAGE_SOURCE)/$(1) OUTPUT=$$(abspath $$@) OBJDIR=$$(abspath $$(OBJ))/package-$(1) V=$$(V)
endef
$(foreach p,$(PACKAGES),$(eval $(call PACKAGE_RULE,$(p))))
$(PACKAGE_CATALOG_TARGET): $(PACKAGE_MANIFEST) | $(PACKAGE_DIR)
	$(Q)printf "  %-7s %s\n" "INSTALL" "$(call __rel,$@)"
	$(Q)cp $< $@
clean:
	$(Q)find $(OBJ) -depth -delete 2>/dev/null || true
