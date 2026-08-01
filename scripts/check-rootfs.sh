#!/bin/sh
set -eu

rootfs=$1
initrd=$2
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT INT TERM

(cd "$rootfs" && find . -mindepth 1 -printf '%P\n' | sort) > "$work/rootfs.list"
tar -tf "$initrd" | sed 's#^\./##; s#/$##' | sed '/^$/d' | sort -u > "$work/initrd.list"
if ! diff -u "$work/rootfs.list" "$work/initrd.list"; then
    echo "initramfs does not match the staged rootfs" >&2
    exit 1
fi
echo "initramfs contains the complete staged rootfs"
