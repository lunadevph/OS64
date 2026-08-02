# OS64 package repository

`packages.json` is the authoritative catalog. Each catalog entry must have a
matching buildable application under `apps/NAME/`. The build validates names,
versions, categories, descriptions, duplicate entries, and missing sources,
then generates the compact catalog consumed by the kernel.

Package executables are staged on the installation media under
`/usr/share/os64/packages`. The manifest is included there for inspection.
`get install` copies a validated catalog payload to persistent `/var/apps` and
sets executable permissions. OS64 currently uses a local installation-media
repository; network repositories, signatures, dependencies, and upgrades are
future work and are not simulated.

Create a package with the freestanding SDK layout:

```text
packages/apps/example/
├── Makefile
└── app.c
```

Then add its metadata to `packages.json`. Use `get install essentials` to
install every entry marked `essential`.
