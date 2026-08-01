#include "tui.h"

static void cp(char *d, const char *s) {
    while ((*d++ = *s++));
}

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!tui_initialize(api)) {
        api->write("netcfg: terminal must be at least 40x15\n");
        return 1;
    }

    struct tui_application *a = tui_app_create("netcfg");
    struct tui_window *w = tui_window_create(0, 0, 66, 22,
        "Network Configuration", TUI_WINDOW_CENTER);

    tui_label_create(w, 2, 2, "Device: net0 (PCnet / RTL8139 emulated)");

    struct tui_widget *dhcp = tui_widget_create(TUI_RADIO, w, 2, 4, 24, 1, "DHCP");
    dhcp->checked = 1;
    struct tui_widget *static_ip = tui_widget_create(TUI_RADIO, w, 2, 5, 24, 1, "Static");
    (void)static_ip;

    tui_label_create(w, 2, 7, "IP Address:");
    struct tui_widget *ip = tui_textbox_create(w, 18, 7, 20, "10.0.2.15", 31, 0);

    tui_label_create(w, 2, 9, "Gateway:");
    struct tui_widget *gw = tui_textbox_create(w, 18, 9, 20, "10.0.2.2", 31, 0);

    tui_label_create(w, 2, 11, "DNS:");
    struct tui_widget *dns = tui_textbox_create(w, 18, 11, 20, "10.0.2.3", 31, 0);

    struct tui_widget *save = tui_button_create(w, 18, 15, 12, "  Save  ");
    struct tui_widget *cancel = tui_button_create(w, 34, 15, 12, " Cancel ");
    (void)cancel;

    tui_widget_create(TUI_STATUS_BAR, w, 1, 18, 60, 1,
        "Tab: Navigate   Space: Toggle   Enter: Save   F10: Exit");

    for (;;) {
        tui_render();
        struct tui_event e;
        tui_next_event(&e);
        if (e.key == TUI_KEY_F10 || e.key == 27 || e.type == TUI_EVENT_CLOSE)
            break;

        if (e.key == '\n' && (dhcp->focused || save->focused)) {
            char msg[128] = "Configuration saved:\n";
            cp(msg + 27, dhcp->checked ? "DHCP mode" : "Static IP");
            cp(msg + 27 + (dhcp->checked ? 9 : 9), "\n");
            if (!dhcp->checked) {
                cp(msg + 27 + 10, "IP: ");
                size_t p = 27 + 14;
                size_t i = 0;
                while (ip->text[i]) msg[p++] = ip->text[i++];
                msg[p++] = '\n';
                cp(msg + p, "GW: ");
                p += 4;
                i = 0;
                while (gw->text[i]) msg[p++] = gw->text[i++];
                msg[p++] = '\n';
                cp(msg + p, "DNS: ");
                p += 5;
                i = 0;
                while (dns->text[i]) msg[p++] = dns->text[i++];
                msg[p] = 0;
            }
            tui_message_box("Network", msg, TUI_BUTTON_OK);
            break;
        }

        if (e.key == ' ' && dhcp->focused) {
            dhcp->checked = 1;
            static_ip->checked = 0;
        } else if (e.key == ' ' && static_ip->focused) {
            static_ip->checked = 1;
            dhcp->checked = 0;
        }

        tui_dispatch_event(a, &e);
    }

    tui_shutdown();
    return 0;
}
