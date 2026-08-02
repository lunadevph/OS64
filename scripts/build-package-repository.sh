#!/usr/bin/env bash
set -euo pipefail

project_root=$(cd "$(dirname "$0")/.." && pwd)
manifest="$project_root/packages/packages.json"
output_dir="$project_root/packages/repository"
build_dir="$project_root/build/package-repository"

mkdir -p "$output_dir" "$build_dir"

python3 - "$manifest" <<'PY' | while IFS= read -r package; do
import json
import pathlib
import sys

document = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
for package in document["packages"]:
    print(package["name"])
PY
    source_dir="$project_root/packages/apps/$package"
    binary="$build_dir/$package"
    object_dir="$build_dir/obj-$package"
    if [[ ! -f "$source_dir/Makefile" ]]; then
        echo "package repository: missing source for $package" >&2
        exit 1
    fi
    make --no-print-directory -C "$source_dir" \
        OUTPUT="$binary" \
        OBJDIR="$object_dir" \
        SDK="$project_root/sdk"
    base64 -w 76 "$binary" > "$output_dir/$package.b64.tmp"
    mv "$output_dir/$package.b64.tmp" "$output_dir/$package.b64"
    printf '  %-7s %s\n' "PACKAGE" "packages/repository/$package.b64"
done
