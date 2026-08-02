#include "app_abi.h"

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!api || api->version != OS64_ABI_VERSION) return 126;
    api->write("OS64 network diagnostics\n\n");
    if (!api->system_query("network.ready")) {
        api->write("Network is offline. Check netd and the emulated adapter.\n");
        return 1;
    }
    if (api->dispatch("ifconfig", "") != 0) return 1;
    api->write("\nRouting table\n");
    if (api->dispatch("route", "") != 0) return 1;
    api->write("\nResolver\n");
    return api->dispatch("nslookup", "time.cloudflare.com");
}
