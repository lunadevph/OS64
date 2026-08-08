#ifndef OS64_CXX_HPP
#define OS64_CXX_HPP
#include <os64.h>

namespace os64 {
class Console {
    const os64_api_t *api_;
public:
    explicit Console(const os64_api_t *api) : api_(api) {}
    Console &operator<<(const char *text) { os64_write(api_, text ? text : "(null)"); return *this; }
    Console &operator<<(char value) { os64_putc(api_, value); return *this; }
};

template<class T> class unique_ptr {
    T *value_;
public:
    explicit unique_ptr(T *value = nullptr) : value_(value) {}
    ~unique_ptr() { delete value_; }
    unique_ptr(const unique_ptr &) = delete;
    unique_ptr &operator=(const unique_ptr &) = delete;
    T *get() const { return value_; }
    T &operator*() const { return *value_; }
    T *operator->() const { return value_; }
    explicit operator bool() const { return value_ != nullptr; }
};
}
#endif
