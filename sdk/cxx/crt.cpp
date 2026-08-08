#include <os64/libc.h>

using function = void (*)();
extern function __init_array_start[];
extern function __init_array_end[];
extern function __fini_array_start[];
extern function __fini_array_end[];

int os64_main(const os64_api_t *, const char *);

extern "C" int _start(const os64_api_t *api, const char *arguments) {
    if (!api || api->version != OS64_ABI_VERSION) return 1;
    os64_libc_init(api);
    for (function *ctor = __init_array_start; ctor != __init_array_end; ++ctor) (*ctor)();
    int status = os64_main(api, arguments ? arguments : "");
    for (function *dtor = __fini_array_end; dtor != __fini_array_start;) (*--dtor)();
    return status;
}
