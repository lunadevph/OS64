#!/bin/sh
set -eu
root=${1:-user}
quiet=${QUIET:-0}
for arg; do
    case "$arg" in
        --quiet|-q) quiet=1; shift ;;
    esac
done
[ "$quiet" = 1 ] && exit 0
printf 'Normal commands: '
sed -n 's/^bin=//p' "$root/commands.conf"
printf 'Administrative commands: '
sed -n 's/^sbin=//p' "$root/commands.conf"
find "$root/bin" "$root/sbin" "$root/games" "$root/apps" -mindepth 2 -maxdepth 2 \
    -name Makefile -type f -printf 'Application: %h\n' 2>/dev/null || true
