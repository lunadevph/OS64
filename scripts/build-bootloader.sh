#!/bin/sh
set -eu

layout=$1
output=$2
work=$3
loader=${4:-grub}
mkdir -p "$work" "$(dirname "$output")"
layout=$(readlink -f "$layout")
output=$(readlink -m "$output")
work=$(readlink -f "$work")

nasm -f bin "$layout" -o "$work/mbr-layout.img"
case "$loader" in
grub)
cat > "$work/early.cfg" <<'EOF'
set root=(hd0,msdos1)
multiboot2 /KERNEL.BIN bootmode=normal
module2 /INITRD.BIN initrd
boot
EOF
;;
os64)
cat > "$work/early.cfg" <<'EOF'
set root=(hd0,msdos1)
normal /GRUB.CFG
EOF
;;
*) echo "bootloader: expected grub or os64, got '$loader'" >&2; exit 2 ;;
esac

grub-mkimage -O i386-pc -C none -p '(hd0,msdos1)/boot/grub' \
    -c "$work/early.cfg" -o "$work/core.img" \
    biosdisk part_msdos fat normal test search search_fs_file serial terminal multiboot2
cp /usr/lib/grub/i386-pc/boot.img "$work/boot.img"

disk="$work/grub-template.img"
truncate -s 128M "$disk"
dd if="$work/mbr-layout.img" of="$disk" conv=notrunc status=none
loop=$(sudo losetup -f)
if [ ! -e "$loop" ]; then
    minor=${loop#/dev/loop}
    sudo mknod "$loop" b 7 "$minor"
fi
sudo losetup "$loop" "$disk"
cleanup() {
    sudo losetup -d "$loop" 2>/dev/null || true
}
trap cleanup EXIT INT TERM
printf '(hd0) %s\n' "$loop" > "$work/device.map"
sudo /usr/lib/grub/i386-pc/grub-bios-setup -f -s \
    -d "$work" -b boot.img -c core.img -m "$work/device.map" "$loop"
sudo chown "$(id -u):$(id -g)" "$disk"
dd if="$disk" of="$output" bs=512 count=2048 status=none
