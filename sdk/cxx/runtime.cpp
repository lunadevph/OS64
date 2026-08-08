#include <stddef.h>
#include <stdlib.h>

void *operator new(size_t size) noexcept { if (void *p = malloc(size ? size : 1)) return p; return nullptr; }
void *operator new[](size_t size) noexcept { return operator new(size); }
void operator delete(void *pointer) noexcept { free(pointer); }
void operator delete[](void *pointer) noexcept { free(pointer); }
void operator delete(void *pointer, size_t) noexcept { free(pointer); }
void operator delete[](void *pointer, size_t) noexcept { free(pointer); }

extern "C" {
void *__dso_handle = nullptr;
int __cxa_guard_acquire(unsigned long long *guard) { return !*(reinterpret_cast<unsigned char *>(guard)); }
void __cxa_guard_release(unsigned long long *guard) { *(reinterpret_cast<unsigned char *>(guard)) = 1; }
void __cxa_guard_abort(unsigned long long *) {}
int __cxa_atexit(void (*)(void *), void *, void *) { return 0; }
[[noreturn]] void __cxa_pure_virtual() { for (;;) {} }
}
