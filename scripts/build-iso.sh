#!/bin/sh
set -eu
kernel=$1
initrd=$2
grub_cfg=$3
stage=$4
output=$5
mini64=${6:-}
bootloader=${7:-}
manager=${8:-}
theme=${9:-}
find "$stage" -depth -delete 2>/dev/null || true
mkdir -p "$stage/boot/grub" "$(dirname "$output")"
cp "$kernel" "$stage/boot/kernel.bin"
cp "$initrd" "$stage/boot/initrd.tar"
if [ -n "$mini64" ] && [ -f "$mini64" ]; then
    mkdir -p "$stage/boot/mini64"
    cp "$mini64" "$stage/boot/mini64/mini64.bin"
fi
if [ -n "$bootloader" ] && [ -f "$bootloader" ]; then
    cp "$bootloader" "$stage/boot/os64-boot.img"
fi
if [ -n "$manager" ] && [ -f "$manager" ]; then
    cp "$manager" "$stage/boot/os64.cfg"
fi
if [ -n "$theme" ] && [ -d "$theme" ]; then
    mkdir -p "$stage/boot/grub/themes/os64"
    cp -a "$theme/." "$stage/boot/grub/themes/os64/"
    python3 scripts/generate-grub-theme.py "$stage/boot/grub/themes/os64"
fi
cp "$grub_cfg" "$stage/boot/grub/grub.cfg"
grub-mkrescue -o "$output" "$stage"
