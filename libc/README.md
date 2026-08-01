# OS64 libc

This freestanding library provides bounded OS64 userland building blocks:
`memcpy`, `memmove`, `memset`, string comparison/copy functions, character
classification, decimal conversion, `putchar`, `puts`, and ABI-backed `malloc`.

Call `os64_libc_init(api)` at application startup. `free()` is currently a
documented no-op because ABI 5 does not expose deallocation; applications
should reuse allocations rather than allocate repeatedly.
