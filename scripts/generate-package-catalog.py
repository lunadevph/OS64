#!/usr/bin/env python3
"""Validate packages/packages.json and emit the kernel catalog header."""
import json
import pathlib
import re
import sys

source, output = map(pathlib.Path, sys.argv[1:3])
repositories_source = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else source
document = json.loads(source.read_text(encoding="utf-8"))
if document.get("schema") != 1 or not isinstance(document.get("packages"), list):
    raise SystemExit("invalid package catalog schema")

repositories_document = json.loads(repositories_source.read_text(encoding="utf-8"))
repositories = repositories_document.get("repositories")
if not isinstance(repositories, list) or not repositories or len(repositories) > 8:
    raise SystemExit("package catalog requires 1-8 repositories")
repository_rows = []
for repository in repositories:
    name = repository.get("name", "")
    host = repository.get("host", "")
    base = repository.get("base", "")
    if not re.fullmatch(r"[a-z][a-z0-9-]{0,15}", name):
        raise SystemExit(f"invalid repository name: {name!r}")
    if not re.fullmatch(r"[A-Za-z0-9.-]{1,95}", host):
        raise SystemExit(f"invalid repository host: {host!r}")
    if not re.fullmatch(r"/[A-Za-z0-9._/-]{1,159}/", base) or ".." in base:
        raise SystemExit(f"invalid repository base: {base!r}")
    repository_rows.append((name, host, base))

seen = set()
rows = []
for package in document["packages"]:
    name = package.get("name", "")
    version = package.get("version", "")
    category = package.get("category", "")
    license_id = package.get("license", "")
    upstream = package.get("upstream", "")
    payload = package.get("payload", "")
    sha256 = package.get("sha256", "")
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
    if not re.fullmatch(r"[0-9a-f]{64}", sha256):
        raise SystemExit(f"invalid SHA-256 digest for {name}")
    if len(description) > 79 or any(c in description for c in '\\"\n\r'):
        raise SystemExit(f"invalid description for {name}")
    if not (source.parent / "apps" / name / "Makefile").is_file():
        raise SystemExit(f"missing package application: {name}")
    seen.add(name)
    rows.append((name, version, category, license_id, upstream, payload, sha256,
                 bool(package.get("essential")), description))

lines = ["/* Generated from packages/packages.json. Do not edit. */",
         "#ifndef OS64_PACKAGE_CATALOG_H", "#define OS64_PACKAGE_CATALOG_H",
         "#define OS64_PACKAGE_REPOSITORIES(X) \\"]
for index, (name, host, base) in enumerate(repository_rows):
    suffix = " \\" if index + 1 < len(repository_rows) else ""
    lines.append(f' X("{name}","{host}","{base}"){suffix}')
lines += ["#define OS64_PACKAGE_REPOSITORY_COUNT %d" % len(repository_rows),
         "#define OS64_PACKAGE_CATALOG(X) \\"]
for index, row in enumerate(rows):
    name, version, category, license_id, upstream, payload, sha256, essential, description = row
    suffix = " \\" if index + 1 < len(rows) else ""
    lines.append(f' X("{name}","{version}","{category}","{license_id}","{upstream}","{payload}","{sha256}",{1 if essential else 0},"{description}"){suffix}')
lines += ["#define OS64_PACKAGE_COUNT %d" % len(rows), "#endif", ""]
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text("\n".join(lines), encoding="utf-8")
