#!/usr/bin/env python3
"""Validate packages/packages.json and emit the kernel catalog header."""
import json
import pathlib
import re
import sys

source, output = map(pathlib.Path, sys.argv[1:3])
document = json.loads(source.read_text(encoding="utf-8"))
if document.get("schema") != 1 or not isinstance(document.get("packages"), list):
    raise SystemExit("invalid package catalog schema")

seen = set()
rows = []
for package in document["packages"]:
    name = package.get("name", "")
    version = package.get("version", "")
    category = package.get("category", "")
    license_id = package.get("license", "")
    upstream = package.get("upstream", "")
    payload = package.get("payload", "")
    description = package.get("description", "")
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,14}", name):
        raise SystemExit(f"invalid package name: {name!r}")
    if name in seen:
        raise SystemExit(f"duplicate package: {name}")
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){0,2}", version):
        raise SystemExit(f"invalid version for {name}")
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,14}", category):
        raise SystemExit(f"invalid category for {name}")
    if not re.fullmatch(r"[A-Za-z0-9.+-]{2,31}", license_id):
        raise SystemExit(f"invalid SPDX license for {name}")
    if len(upstream) > 95 or any(c in upstream for c in '\\"\n\r'):
        raise SystemExit(f"invalid upstream for {name}")
    if not re.fullmatch(r"packages/repository/[a-z][a-z0-9-]{0,14}\.b64", payload):
        raise SystemExit(f"invalid payload path for {name}")
    if len(description) > 79 or any(c in description for c in '\\"\n\r'):
        raise SystemExit(f"invalid description for {name}")
    if not (source.parent / "apps" / name / "Makefile").is_file():
        raise SystemExit(f"missing package application: {name}")
    seen.add(name)
    rows.append((name, version, category, license_id, upstream, payload,
                 bool(package.get("essential")), description))

lines = ["/* Generated from packages/packages.json. Do not edit. */",
         "#ifndef OS64_PACKAGE_CATALOG_H", "#define OS64_PACKAGE_CATALOG_H",
         "#define OS64_PACKAGE_CATALOG(X) \\"]
for index, row in enumerate(rows):
    name, version, category, license_id, upstream, payload, essential, description = row
    suffix = " \\" if index + 1 < len(rows) else ""
    lines.append(f' X("{name}","{version}","{category}","{license_id}","{upstream}","{payload}",{1 if essential else 0},"{description}"){suffix}')
lines += ["#define OS64_PACKAGE_COUNT %d" % len(rows), "#endif", ""]
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("\n".join(lines), encoding="utf-8")
