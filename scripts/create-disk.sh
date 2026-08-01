#!/bin/sh
set -eu
output=$1
size=${2:-128M}
mkdir -p "$(dirname "$output")"
truncate -s "$size" "$output"
