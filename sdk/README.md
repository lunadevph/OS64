# OS64 application SDK

OS64 applications are freestanding x86-64 ELF executables loaded at `0x800000`.
They export `_start(const os64_api_t *, const char *)` and link the bundled
minimal libc by default.

ABI v5 provides command dispatch, console output, VFS reads, protected writes
below `/home`, `/mnt/os64`, or `/var/tmp`, a small kernel allocator, CMOS-backed
date/time, and the current user and working directory. Include `os64.h` for
safe inline wrappers or `app_abi.h` for the raw function table.

The official terminal SDK is under `include/os64/`. It provides buffered
Unicode terminal drawing, windows, labels, progress/status bars, filesystem,
process, time, networking, random, configuration, and logging interfaces.
`os_terminal_flush()` updates dirty cells only. Build the complete example with
`make -C sdk/examples/terminal`.

Current limitations are explicit: process spawning, user threads, and direct
application sockets return failure until OS64 gains scheduler/socket syscalls.
Existing kernel commands remain accessible through `os_process_run()`.

An application folder normally contains `app.c` and:

```make
SDK := ../../sdk
include $(SDK)/app.mk
```

Set `SOURCES := app.c module.c` for multiple translation units. Applications
must validate `api->version == OS64_ABI_VERSION` before using the table.

Native freestanding C++17 applications are supported with `g++`. Set
`SOURCES := app.cpp` and implement `int os64_main(const os64_api_t *, const
char *)`; the SDK supplies `_start`, initializes libc, runs global constructors
and destructors, and provides `new`/`delete` plus essential freestanding C++ ABI
hooks. Exceptions, RTTI, and the hosted C++ standard library are intentionally
disabled. See `sdk/examples/cpp` and build it with
`make -C sdk/examples/cpp`.

Long-running applications should periodically call
`api->system_query("process.interrupted")`. A nonzero result means Ctrl+C was
pressed for the foreground application; clean up and return status 130.
