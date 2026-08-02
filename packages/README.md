# OS64 package repository

`packages.json` is the authoritative catalog. Each catalog entry must have a
matching buildable application under `apps/NAME/`. The build validates names,
versions, categories, descriptions, duplicate entries, and missing sources,
then generates the compact catalog consumed by the kernel.

`pm install` downloads a base64-encoded OS64 ELF over certificate-validated
HTTPS from `raw.githubusercontent.com`, validates the response and x86_64 ELF
header, and writes it to persistent `/var/apps`. Package executables are not
bundled in the initramfs. The checked-in `repository/*.b64` files are the
content served by the online repository, not local installation fallbacks.

Create a package with the freestanding SDK layout:

```text
packages/apps/example/
├── Makefile
└── app.c
```

Then add its metadata to `packages.json` and run `make package-repository` to
regenerate the remotely served payloads. Use `pm update` to fetch and cache the
online catalog and `pm install essentials` to install essential entries.

## Upstream ports

- `c2048` adapts Maurits van der Schee's MIT-licensed `2048.c` 1.0.3.
- `sectorlisp` adapts Justine Tunney's ISC-licensed portable SectorLISP
  reference implementation.

Each port retains its license and a README describing which hosted interfaces
were replaced by OS64 ABI calls.
