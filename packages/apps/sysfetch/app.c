#include "app_abi.h"

static void number(const os64_api_t *api, unsigned long n) {
    char digits[24]; unsigned used = 0;
    if (!n) digits[used++] = '0';
    while (n) { digits[used++] = (char)('0' + n % 10); n /= 10; }
    while (used) api->putc(digits[--used]);
}

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!api || api->version != OS64_ABI_VERSION) return 126;
    api->write("OS64 1.0 x86_64\nUser: "); api->write(api->current_user());
    api->write("\nMemory: "); number(api, api->system_query("memory.total_kib") / 1024);
    api->write(" MiB usable\nNetwork: ");
    api->write(api->system_query("network.ready") ? "connected\n" : "offline\n");
    return 0;
}
