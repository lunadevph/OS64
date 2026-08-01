#ifndef OS64_PANIC_H
#define OS64_PANIC_H
__attribute__((noreturn)) void kernel_panic(const char *reason);
#endif
