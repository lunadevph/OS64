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
        "timed", "diskd", "userd", "acpid", "netd"
    };
    static char svc_rows[9][32];
    static const char *svc_items[9];
    for (unsigned i = 0; i < 9; i++)
        svc_items[i] = svc_rows[i];

    struct tui_widget *tbl = tui_table_create(w, 2, 3, 31, 10, svc_items, 9);

    struct tui_widget *info = tui_widget_create(TUI_TEXT_VIEW, w, 37, 3, 35, 10, "");
    info->width = 35;

    tui_widget_create(TUI_STATUS_BAR, w, 1, 21, 74, 1,
        "F1 Help  F5 Refresh  F10 Exit");

    for (;;) {
        for (unsigned i = 0; i < 9; i++) {
            cp(svc_rows[i], svc_names[i]);
            size_t p = 0;
            while (svc_rows[i][p]) p++;
            while (p < 16) svc_rows[i][p++] = ' ';
            char q[32] = "service.";
            cp(q + 8, svc_names[i]);
            unsigned long status = api->system_query(q);
            cp(svc_rows[i] + p, status ? "OK" : "FAIL");
        }

        char ib[128];
        cp(ib, "Kernel: ");
        number(ib + 8, api->system_query("kernel.version"));

        cp(ib + 8 + 1, "\nMemory: ");
        number(ib + 8 + 9, api->system_query("memory.total_kib") / 1024);
        cp(ib + 8 + 9 + 1, " MB");

        cp(ib + 8 + 9 + 5, "\nNetwork: ");
        cp(ib + 8 + 9 + 5 + 9, api->system_query("network.ready") ? "Connected" : "Offline");

        cp(ib + 8 + 9 + 5 + 9 + 9, "\nStorage: ");
        cp(ib + 8 + 9 + 5 + 9 + 9 + 9, api->system_query("disk.mounted") ? "Mounted" : "Unavailable");
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
        else
            tui_dispatch_event(a, &e);

        (void)tbl;
    }

    tui_shutdown();
    return 0;
}
