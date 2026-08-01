#!/bin/sh
set -eu
kernel=$1
initrd=$2
grub_cfg=$3
stage=$4
output=$5
mini64=${6:-}
bootloader=${7:-}
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
cp "$grub_cfg" "$stage/boot/grub/grub.cfg"
grub-mkrescue -o "$output" "$stage"
