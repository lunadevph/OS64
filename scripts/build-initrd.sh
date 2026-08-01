#!/bin/sh
set -eu
rootfs=$1
output=$2
mkdir -p "$(dirname "$output")"
tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
    --mtime='UTC 2026-01-01' -cf "$output" -C "$rootfs" .
