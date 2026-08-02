# SPDX-License-Identifier: GPL-2.0-or-later
#
# OS64 top-level build system
#

PROJECT          := os64
include build.cfg
VERSION          := $(KERNEL_VERSION)

BUILD            ?= build
OBJ              := $(BUILD)/obj
STAGED_ROOTFS    := $(BUILD)/rootfs
INITRD_DIR       := $(BUILD)/initrd
INITRD           := $(INITRD_DIR)/initrd.tar
ISO_STAGE        := $(BUILD)/iso
IMAGES           := $(BUILD)/images

KERNEL_BUILD     := $(OBJ)/kernel
KERNEL           := $(KERNEL_BUILD)/kernel.bin
MINI64_SOURCE      := mini64
MINI64_BUILD       := $(OBJ)/mini64
MINI64             := $(MINI64_BUILD)/mini64.bin
ISO              := $(IMAGES)/$(PROJECT).iso
DISK             := $(IMAGES)/$(PROJECT)-disk.img
HOME_IMAGE       := $(OBJ)/$(PROJECT)-home.fat32.img

ROOTFS_SOURCE    := rootfs
KERNEL_SOURCE    := kernel
USER_SOURCE      := user

GENERATED        := $(BUILD)/generated
BUILD_HEADER     := $(GENERATED)/build_config.h
GRUB_CONFIG      := $(GENERATED)/grub.cfg
GENERATED_MOTD   := $(GENERATED)/motd
PACKAGE_MANIFEST := packages/packages.json
PACKAGE_CATALOG  := $(GENERATED)/package_catalog.h
BOOTLOADER_SRC   := boot/mbr-layout.asm
BOOTLOADER_IMAGE := $(STAGED_ROOTFS)/boot/os64-boot.img

DISK_SIZE        ?= 128M
MEMORY           ?= 128M
CPU              ?= max
NETWORK_MODEL    ?= rtl8139

QEMU             ?= qemu-system-x86_64
QEMU_COMMON      := -cpu $(CPU) \
                    -m $(MEMORY) \
                    -drive file=$(DISK),format=raw,if=ide \
                    -nic user,model=$(NETWORK_MODEL)

# ---------------------------------------------------------------------------
# Build verbosity
#
# Default:
#   make
#
# Verbose:
#   make V=1
# ---------------------------------------------------------------------------

ifeq ($(V),1)
Q :=
quiet :=
else
Q := @
quiet := quiet_
MAKEFLAGS += --no-print-directory
endif

quiet_cmd_MKDIR   = MKDIR   $@
      cmd_MKDIR   = mkdir -p $@

quiet_cmd_COPY    = COPY    $@
      cmd_COPY    = cp $< $@

quiet_cmd_ROOTFS  = ROOTFS  $(STAGED_ROOTFS)
      cmd_ROOTFS  = \
	rm -rf $(STAGED_ROOTFS); \
	mkdir -p $(STAGED_ROOTFS); \
	cp -a $(ROOTFS_SOURCE)/. $(STAGED_ROOTFS)/; \
	mkdir -p \
		$(STAGED_ROOTFS)/boot \
		$(STAGED_ROOTFS)/bin \
		$(STAGED_ROOTFS)/sbin \
		$(STAGED_ROOTFS)/dev \
		$(STAGED_ROOTFS)/proc \
		$(STAGED_ROOTFS)/sys \
		$(STAGED_ROOTFS)/run \
		$(STAGED_ROOTFS)/home \
		$(STAGED_ROOTFS)/root \
		$(STAGED_ROOTFS)/tmp \
		$(STAGED_ROOTFS)/media \
		$(STAGED_ROOTFS)/opt \
		$(STAGED_ROOTFS)/srv \
		$(STAGED_ROOTFS)/usr/bin \
		$(STAGED_ROOTFS)/usr/lib \
		$(STAGED_ROOTFS)/usr/sbin \
		$(STAGED_ROOTFS)/usr/share/man \
		$(STAGED_ROOTFS)/usr/share/games \
		$(STAGED_ROOTFS)/var/cache \
		$(STAGED_ROOTFS)/var/lib \
		$(STAGED_ROOTFS)/var/log \
		$(STAGED_ROOTFS)/var/run \
		$(STAGED_ROOTFS)/var/tmp

quiet_cmd_SCAN    = SCAN    userspace applications
      cmd_SCAN    = ./scripts/scan-apps.sh user $(if $(filter 1,$(V)),,--quiet)

quiet_cmd_USER    = USER    userspace
      cmd_USER    = $(MAKE) -C $(USER_SOURCE) \
		ROOTFS=$(abspath $(STAGED_ROOTFS)) \
		OBJ=$(abspath $(OBJ))/user \
		CONFIG_HEADER=$(abspath $(BUILD_HEADER)) \
		V=$(V)

quiet_cmd_LIBC    = LIBC    $(OBJ)/libc/libos64c.a
      cmd_LIBC    = $(MAKE) -C libc BUILD=$(abspath $(OBJ))/libc OUTPUT=$(abspath $(OBJ))/libc/libos64c.a V=$(V)

quiet_cmd_PKGCAT  = PKGCAT  $(PACKAGE_MANIFEST)
      cmd_PKGCAT  = python3 scripts/generate-package-catalog.py $(PACKAGE_MANIFEST) $(PACKAGE_CATALOG)

quiet_cmd_KERNEL  = KERNEL  $(KERNEL)
      cmd_KERNEL  = $(MAKE) -C $(KERNEL_SOURCE) \
		BUILD=$(abspath $(KERNEL_BUILD)) \
		OUTPUT=$(abspath $(KERNEL)) \
		CONFIG_HEADER=$(abspath $(BUILD_HEADER)) \
		PACKAGE_HEADER=$(abspath $(PACKAGE_CATALOG)) \
		V=$(V)

quiet_cmd_MINI64    = MINI64  $(MINI64)
      cmd_MINI64    = $(MAKE) -C $(MINI64_SOURCE) \
		BUILD=$(abspath $(MINI64_BUILD)) \
		OUTPUT=$(abspath $(MINI64)) \
		V=$(V)

quiet_cmd_BOOT    = BOOT    $(BOOTLOADER_IMAGE)
      cmd_BOOT    = ./scripts/build-bootloader.sh \
		$(BOOTLOADER_SRC) \
		$(BOOTLOADER_IMAGE) \
		$(OBJ)/bootloader

quiet_cmd_INITRD  = INITRD  $(INITRD)
      cmd_INITRD  = ./scripts/build-initrd.sh \
		$(STAGED_ROOTFS) \
		$(INITRD)

quiet_cmd_ISO     = ISO     $(ISO)
      cmd_ISO     = ./scripts/build-iso.sh \
		$(KERNEL) \
		$(INITRD) \
		$(GRUB_CONFIG) \
		$(ISO_STAGE) \
		$(ISO) \
		$(MINI64) \
		$(BOOTLOADER_IMAGE)

quiet_cmd_DISK    = DISK    $(DISK)
      cmd_DISK    = ./scripts/create-disk.sh $(DISK) $(DISK_SIZE)

quiet_cmd_CHECK   = CHECK   $(KERNEL)
      cmd_CHECK   = grub-file --is-x86-multiboot2 $(KERNEL)

quiet_cmd_ROOTCHK = CHECK   root filesystem
      cmd_ROOTCHK = ./scripts/check-rootfs.sh \
		$(STAGED_ROOTFS) \
		$(INITRD)

quiet_cmd_FATIMG  = EXTRACT $(HOME_IMAGE)
      cmd_FATIMG  = dd \
		if=$(DISK) \
		of=$(HOME_IMAGE) \
		bs=512 \
		skip=2048 \
		count=129024 \
		status=none

quiet_cmd_FATCHK  = FSCK    $(HOME_IMAGE)
      cmd_FATCHK  = \
	if od -An -tu2 -j11 -N2 $(HOME_IMAGE) | tr -d ' ' | grep -qx 512; then \
		fsck.fat -n -v $(HOME_IMAGE); \
	else \
		printf "  %-7s %s\n" "SKIP" "data disk is blank; run the OS64 installer before FAT validation"; \
	fi

quiet_cmd_CLEAN   = CLEAN   $(BUILD)
      cmd_CLEAN   = rm -rf $(BUILD)

# Print a Linux-style command status line, then execute the command.
# With V=1, print full commands instead of status lines.
ifeq ($(V),1)
cmd = $(cmd_$(1))
else
cmd = @printf "  %s\n" "$(quiet_cmd_$(1))"; \
	$(cmd_$(1))
endif

# ---------------------------------------------------------------------------
# Primary targets
# ---------------------------------------------------------------------------

.PHONY: all
all: images

.PHONY: images
images: iso disk
	@printf "\n"
	@printf "$(OS_NAME) $(VERSION) build complete\n"
	@printf "  Kernel: %s\n" "$(KERNEL)"
	@printf "  ISO:    %s\n" "$(ISO)"
	@printf "  Disk:   %s\n" "$(DISK)"

# ---------------------------------------------------------------------------
# Directory targets
# ---------------------------------------------------------------------------

$(BUILD) $(OBJ) $(IMAGES) $(INITRD_DIR) $(ISO_STAGE):
	$(call cmd,MKDIR)

# ---------------------------------------------------------------------------
# Root filesystem
# ---------------------------------------------------------------------------

.PHONY: stage-rootfs
stage-rootfs: $(STAGED_ROOTFS)

$(STAGED_ROOTFS):
	$(call cmd,ROOTFS)

.PHONY: config
config: stage-rootfs
	$(Q)printf "  %s\n" "CONFIG  build.cfg"
	$(Q)./scripts/gen-build-config.sh build.cfg $(BUILD_HEADER) $(GRUB_CONFIG) $(GENERATED_MOTD)
	$(Q)cp $(GENERATED_MOTD) $(STAGED_ROOTFS)/etc/motd

.PHONY: scan-apps
scan-apps:
	$(call cmd,SCAN)

# ---------------------------------------------------------------------------
# Userspace
# ---------------------------------------------------------------------------

.PHONY: user
user: stage-rootfs scan-apps config package-catalog libc
	$(call cmd,USER)

.PHONY: package-catalog
package-catalog: $(PACKAGE_CATALOG)

$(PACKAGE_CATALOG): $(PACKAGE_MANIFEST) scripts/generate-package-catalog.py | $(BUILD)
	$(call cmd,PKGCAT)

.PHONY: libc
libc: | $(OBJ)
	$(call cmd,LIBC)

# ---------------------------------------------------------------------------
# Kernel
# ---------------------------------------------------------------------------

.PHONY: kernel
kernel: user config package-catalog
	$(call cmd,KERNEL)
	$(Q)printf "  %s\n" "COPY    $(STAGED_ROOTFS)/boot/kernel.bin"
	$(Q)mkdir -p $(STAGED_ROOTFS)/boot
	$(Q)cp $(KERNEL) $(STAGED_ROOTFS)/boot/kernel.bin

.PHONY: mini64
mini64: kernel
	$(call cmd,MINI64)
	$(Q)printf "  %s\n" "COPY    $(STAGED_ROOTFS)/boot/mini64.bin"
	$(Q)mkdir -p $(STAGED_ROOTFS)/boot/mini64
	$(Q)cp $(MINI64) $(STAGED_ROOTFS)/boot/mini64/mini64.bin

# ---------------------------------------------------------------------------
# Bootloader
# ---------------------------------------------------------------------------

.PHONY: bootloader
bootloader: kernel
	$(call cmd,BOOT)

# ---------------------------------------------------------------------------
# Initramfs
# ---------------------------------------------------------------------------

.PHONY: initrd
initrd: bootloader | $(INITRD_DIR)
	$(call cmd,INITRD)

# ---------------------------------------------------------------------------
# ISO image
# ---------------------------------------------------------------------------

.PHONY: iso
iso: initrd mini64 | $(IMAGES) $(ISO_STAGE)
	$(call cmd,ISO)

# ---------------------------------------------------------------------------
# Persistent disk image
# ---------------------------------------------------------------------------

.PHONY: disk
disk: $(DISK)

$(DISK): | $(IMAGES)
	$(call cmd,DISK)

.PHONY: disk-reset
disk-reset:
	$(Q)rm -f $(DISK)
	$(MAKE) disk

# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------

.PHONY: check
check: kernel
	$(call cmd,CHECK)
	@printf "  %s\n" "OK      OS64 kernel is a valid Multiboot2 x86-64 image"

.PHONY: check-rootfs
check-rootfs: initrd
	$(call cmd,ROOTCHK)

.PHONY: check-fat
check-fat: disk | $(OBJ)
	$(call cmd,FATIMG)
	$(call cmd,FATCHK)

.PHONY: check-all
check-all: check check-rootfs check-fat

.PHONY: smoke-commands
smoke-commands: iso
	$(Q)python3 scripts/smoke-commands.py

.PHONY: generate-screenshots
generate-screenshots: iso disk
	$(Q)python3 scripts/generate-screenshots.py

.PHONY: check-ext
check-ext: iso
	$(Q)python3 scripts/check-ext-variants.py

# ---------------------------------------------------------------------------
# QEMU
# ---------------------------------------------------------------------------

.PHONY: run
run: iso disk
	@printf "  %-7s %s\n" "QEMU" "serial console"
	$(Q)$(QEMU) \
		$(QEMU_COMMON) \
		-boot order=d \
		-cdrom $(ISO) \
		-serial stdio \
		-display none

.PHONY: run-console
run-console: iso disk
	@printf "  %-7s %s\n" "QEMU" "nographic console"
	$(Q)$(QEMU) \
		$(QEMU_COMMON) \
		-boot order=d \
		-cdrom $(ISO) \
		-nographic \
		-monitor none

.PHONY: run-installed
run-installed: disk
	@printf "  %-7s %s\n" "QEMU" "installed system"
	$(Q)$(QEMU) \
		$(QEMU_COMMON) \
		-boot order=c \
		-nographic \
		-monitor none

.PHONY: run-gui
run-gui: iso disk
	@printf "  %-7s %s\n" "QEMU" "graphical display"
	$(Q)$(QEMU) \
		$(QEMU_COMMON) \
		-boot order=d \
		-cdrom $(ISO) \
		-serial stdio

.PHONY: run-pcnet
run-pcnet: iso disk
	@printf "  %-7s %s\n" "QEMU" "AMD PCnet network"
	$(Q)$(QEMU) \
		-cpu $(CPU) \
		-m $(MEMORY) \
		-boot order=d \
		-cdrom $(ISO) \
		-drive file=$(DISK),format=raw,if=ide \
		-nic user,model=pcnet \
		-serial stdio

.PHONY: debug
debug: iso disk
	@printf "  %-7s %s\n" "QEMU" "waiting for GDB on :1234"
	$(Q)$(QEMU) \
		$(QEMU_COMMON) \
		-boot order=d \
		-cdrom $(ISO) \
		-serial stdio \
		-display none \
		-s \
		-S

# ---------------------------------------------------------------------------
# Developer utilities
# ---------------------------------------------------------------------------

.PHONY: rebuild
rebuild:
	$(MAKE) clean
	$(MAKE) all

.PHONY: print-config
print-config:
	@printf "OS64 build configuration\n"
	@printf "  PROJECT          = %s\n" "$(PROJECT)"
	@printf "  VERSION          = %s\n" "$(VERSION)"
	@printf "  BUILD            = %s\n" "$(BUILD)"
	@printf "  KERNEL           = %s\n" "$(KERNEL)"
	@printf "  INITRD           = %s\n" "$(INITRD)"
	@printf "  ISO              = %s\n" "$(ISO)"
	@printf "  DISK             = %s\n" "$(DISK)"
	@printf "  DISK_SIZE        = %s\n" "$(DISK_SIZE)"
	@printf "  QEMU             = %s\n" "$(QEMU)"
	@printf "  CPU              = %s\n" "$(CPU)"
	@printf "  MEMORY           = %s\n" "$(MEMORY)"
	@printf "  NETWORK_MODEL    = %s\n" "$(NETWORK_MODEL)"

.PHONY: help
help:
	@printf "OS64 build system\n\n"
	@printf "Usage:\n"
	@printf "  make [target] [V=1]\n\n"
	@printf "Build targets:\n"
	@printf "  all              Build bootable ISO and disk image\n"
	@printf "  kernel           Build userspace and kernel\n"
	@printf "  user             Build userspace programs\n"
	@printf "  bootloader       Build the OS64 bootloader\n"
	@printf "  initrd           Build the initramfs archive\n"
	@printf "  iso              Build the bootable ISO image\n"
	@printf "  disk             Create the persistent disk image\n"
	@printf "  disk-reset       Recreate the persistent disk image\n"
	@printf "\n"
	@printf "Validation targets:\n"
	@printf "  check            Validate the Multiboot2 kernel\n"
	@printf "  check-rootfs     Validate the staged root filesystem\n"
	@printf "  check-fat        Check the FAT32 disk filesystem\n"
	@printf "  check-ext        Create and validate ext2/ext3/ext4 filesystems\n"
	@printf "  check-all        Run every validation target\n"
	@printf "  generate-screenshots  Capture README images from QEMU\n"
	@printf "\n"
	@printf "Run targets:\n"
	@printf "  run              Run with serial output only\n"
	@printf "  run-console      Run using QEMU's nographic console\n"
	@printf "  run-installed    Boot from the persistent disk\n"
	@printf "  run-gui          Run with a graphical window\n"
	@printf "  run-pcnet        Run using the AMD PCnet adapter\n"
	@printf "  debug            Wait for a GDB connection on port 1234\n"
	@printf "\n"
	@printf "Maintenance targets:\n"
	@printf "  clean            Remove generated files\n"
	@printf "  rebuild          Clean and rebuild everything\n"
	@printf "  print-config     Display the active configuration\n"
	@printf "\n"
	@printf "Examples:\n"
	@printf "  make\n"
	@printf "  make -j$$(nproc)\n"
	@printf "  make V=1\n"
	@printf "  make run-gui MEMORY=256M\n"
	@printf "  make run NETWORK_MODEL=pcnet\n"

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------

.PHONY: clean
clean:
	$(call cmd,CLEAN)

.PHONY: mrproper
mrproper: clean
	$(Q)rm -f .config
	@printf "  %-7s %s\n" "CLEAN" "configuration files"
