#!/bin/sh
set -eu
config=${1:?build.cfg is required}
header=${2:?header output is required}
grub=${3:?GRUB output is required}
motd=${4:?MOTD output is required}
value() {
    key=$1
    result=$(sed -n "s/^${key}=//p" "$config" | tail -n 1)
    [ -n "$result" ] || { echo "build config: missing ${key}" >&2; exit 1; }
    case $result in *'"'*|*'\\'*) echo "build config: unsafe ${key}" >&2; exit 1;; esac
    printf '%s' "$result"
}
name=$(value OS_NAME)
version=$(value KERNEL_VERSION)
description=$(value OS_DESCRIPTION)
policy=$(value SYSTEM_POLICY)
arch=$(value ARCHITECTURE)
mkdir -p "$(dirname "$header")" "$(dirname "$grub")" "$(dirname "$motd")"
sed -e "s/@OS_NAME@/$name/g" -e "s/@KERNEL_VERSION@/$version/g" \
    -e "s/@OS_DESCRIPTION@/$description/g" -e "s/@SYSTEM_POLICY@/$policy/g" \
    -e "s/@ARCHITECTURE@/$arch/g" scripts/build_config.h.in > "$header"
sed -e "s/OS64 1\.0/$name $version/g" boot/grub/grub.cfg > "$grub"
sed -e "s/OS64 1\.0/$name $version/g" \
    -e "s/x86_64/$arch/g" rootfs/etc/motd > "$motd"
