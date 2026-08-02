#include "tui.h"

static void cp(char *d, const char *s) {
    while ((*d++ = *s++));
}

static void number(char *d, unsigned long n) {
    char r[24];
    unsigned i = 0, j = 0;
    if (!n) r[i++] = '0';
    while (n) { r[i++] = (char)('0' + n % 10); n /= 10; }
    while (i) d[j++] = r[--i];
    d[j] = 0;
}

static void append(char *d, size_t cap, const char *s) {
    size_t n = 0;
    while (n < cap && d[n]) n++;
    while (*s && n + 1 < cap) d[n++] = *s++;
    if (n < cap) d[n] = 0;
}

static void append_number(char *d, size_t cap, unsigned long n) {
    char value[24];
    number(value, n);
    append(d, cap, value);
}

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!tui_initialize(api)) {
        api->write("sysmgr: terminal must be at least 40x15\n");
        return 1;
    }

    struct tui_application *a = tui_app_create("sysmgr");
    struct tui_window *w = tui_window_create(0, 0, 80, 25, "OS64 System Manager", 0);

    tui_widget_create(TUI_MENU_BAR, w, 1, 0, 74, 1,
        " File  System  Network  Users  Help");

    static const char *svc_names[] = {
        "fsd", "memoryd", "displayd", "graphicsd",
        "timed", "diskd", "userd", "acpid", "netd", "logd"
    };
    static char svc_rows[10][32];
    static const char *svc_items[10];
    for (unsigned i = 0; i < 10; i++)
        svc_items[i] = svc_rows[i];

    struct tui_widget *tbl = tui_table_create(w, 2, 3, 31, 11, svc_items, 10);

    struct tui_widget *info = tui_widget_create(TUI_TEXT_VIEW, w, 37, 3, 35, 12, "");
    info->width = 35;

    tui_widget_create(TUI_STATUS_BAR, w, 1, 21, 74, 1,
        "F1 Help  F5 Refresh  F10 Exit");

    for (;;) {
        for (unsigned i = 0; i < 10; i++) {
            cp(svc_rows[i], svc_names[i]);
            size_t p = 0;
            while (svc_rows[i][p]) p++;
            while (p < 16) svc_rows[i][p++] = ' ';
            char q[32] = "service.";
            cp(q + 8, svc_names[i]);
            unsigned long status = api->system_query(q);
            cp(svc_rows[i] + p, status ? "OK" : "FAIL");
        }

        char ib[128] = "Hostname: os64\nKernel:   1.0 x86_64\nMemory:   ";
        append_number(ib, sizeof ib, api->system_query("memory.total_kib") / 1024);
        append(ib, sizeof ib, " MiB\nHeap free:");
        append_number(ib, sizeof ib, api->system_query("memory.free_bytes") / 1024);
        append(ib, sizeof ib, " KiB\nNetwork:  ");
        append(ib, sizeof ib, api->system_query("network.ready") ? "connected" : "offline");
        append(ib, sizeof ib, "\n/home:    ");
        append(ib, sizeof ib, api->system_query("disk.mounted") ? "mounted" : "unavailable");
        append(ib, sizeof ib, "\n/var:     ");
        append(ib, sizeof ib, api->system_query("var.mounted") ? "mounted" : "unavailable");
        append(ib, sizeof ib, "\nUsers:    ");
        append_number(ib, sizeof ib, api->system_query("users.count"));
        cp(info->text, ib);

        tui_request_redraw();
        tui_render();

        struct tui_event e;
        tui_next_event(&e);
        if (e.key == TUI_KEY_F10 || e.key == 27 || e.type == TUI_EVENT_CLOSE)
            break;
        if (e.key == TUI_KEY_F1)
            tui_message_box("Help", "F5 refreshes live service status.\nF10 returns to the shell.",
                TUI_BUTTON_OK);
        else if (e.key == TUI_KEY_F5)
            tui_request_redraw();
        else
            tui_dispatch_event(a, &e);

        (void)tbl;
    }

    tui_shutdown();
    return 0;
}
