# Microsoft Edit port status

Upstream: https://github.com/microsoft/edit

Audited revision: `9b34f1876966d834453cba39968f97137d0471b4`

Microsoft Edit 2.0 is MIT licensed and written in Rust 2024. A direct build is
not yet possible on OS64: upstream requires Rust 1.93, the hosted Rust standard
library, POSIX file descriptors, termios, signals, poll/ppoll, monotonic time,
threads, dynamic ICU loading, and platform SIMD detection. OS64 ABI v7 exposes
a callback table to freestanding ELF applications rather than those APIs.

`/usr/bin/edit` is the first compatibility milestone. It is a native OS64
frontend with the upstream project's approachable full-screen layout and uses
terminal ownership, keyboard input, VFS reads/writes, cursor handling, and
kernel permission enforcement. It supports new/existing files, insertion,
deletion, arrows, Home/End, Page Up/Page Down, Ctrl+S, and guarded Ctrl+Q.

This is deliberately not described as a source-complete upstream port. Next:

1. Add a Tier-3 `x86_64-unknown-os64` Rust target with `core` and `alloc`.
2. Add ABI-backed Rust allocation, panic, VFS, terminal, input, and time layers.
3. Implement upstream `crates/edit/src/sys/os64.rs`.
4. Route direct `std::fs::File` users through that platform abstraction.
5. Initially disable ICU and portable SIMD, then port them separately.
