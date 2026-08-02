# NASM port status

OS64 does **not** currently ship a program named `nasm`. The official NASM
3.02 source is an established, BSD-2-Clause assembler, but presenting a small
custom encoder as NASM would be misleading and incompatible.

The upstream source build assumes an ANSI C environment and a substantially
larger hosted runtime than OS64 currently exposes. The blocking OS64 work is:

- seekable file descriptors and complete `fopen`/`fread`/`fwrite`/`fseek`;
- dynamic allocation suitable for large, long-lived compiler data structures;
- process arguments and environment variables beyond the compact app ABI;
- temporary files, path search, include files, and robust error streams;
- larger executable loading (the package transport is currently capped at
  32 KiB and the app loader has a fixed 8 MiB virtual-address window);
- the generated instruction tables from the official NASM release tarball;
- conformance tests for `bin` and ELF64 output before publishing a package.

The port must use the unmodified official 3.02 release tarball as its baseline,
retain the upstream license, and begin with `-f bin` and `-f elf64`. It becomes
eligible for `packages/packages.json` only after it assembles NASM's upstream
test corpus and its output compares byte-for-byte with a host NASM build.

Upstream references:

- <https://www.nasm.us/>
- <https://www.nasm.us/doc/nasmad.html>
- <https://github.com/netwide-assembler/nasm>

This status file is intentionally explicit so the package manager cannot offer
a non-working or falsely labelled assembler.
