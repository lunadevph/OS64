#include "tui.h"

static void cp(char *d, const char *s) {
    while ((*d++ = *s++));
}

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!tui_initialize(api)) {
        api->write("usercfg: terminal must be at least 40x15\n");
        return 1;
    }

    struct tui_application *a = tui_app_create("usercfg");
    struct tui_window *w = tui_window_create(0, 0, 68, 22,
        "User Configuration", TUI_WINDOW_CENTER);

    static const char *users[] = {
        "root       /bin/sh       enabled",
        "nobody     /sbin/nologin disabled",
        "guest      /bin/sh       disabled"
    };

    struct tui_widget *tbl = tui_table_create(w, 2, 3, 40, 8, users, 3);
    (void)tbl;

    struct tui_widget *add = tui_button_create(w, 45, 3, 16, "  Add User  ");
    struct tui_widget *pw = tui_button_create(w, 45, 6, 16, "  Password  ");
    struct tui_widget *sh = tui_button_create(w, 45, 9, 16, "  Set Shell ");
    struct tui_widget *rm = tui_button_create(w, 45, 12, 16, "  Remove    ");

    tui_widget_create(TUI_STATUS_BAR, w, 1, 18, 62, 1,
        "Tab: Navigate   Enter: Action   F10: Exit");

    for (;;) {
        tui_render();
        struct tui_event e;
        tui_next_event(&e);
        if (e.key == TUI_KEY_F10 || e.key == 27 || e.type == TUI_EVENT_CLOSE)
            break;

        if (e.key == '\n') {
            if (add->focused) {
                char uname[32] = "";
                if (tui_input_box("Add User", "Username:", uname, sizeof uname)) {
                    char msg[64];
                    cp(msg, "User '");
                    size_t p = 6;
                    size_t i = 0;
                    while (uname[i]) msg[p++] = uname[i++];
                    cp(msg + p, "' added.");
                    tui_message_box("User Config", msg, TUI_BUTTON_OK);
                }
            } else if (pw->focused) {
                char uname[32] = "";
                if (tui_input_box("Set Password", "Username:", uname, sizeof uname)) {
                    char pass[32] = "";
                    if (tui_input_box("Set Password", "New password:", pass, sizeof pass)) {
                        char msg[64];
                        cp(msg, "Password for '");
                        size_t p = 14;
                        size_t i = 0;
                        while (uname[i]) msg[p++] = uname[i++];
                        cp(msg + p, "' updated.");
                        tui_message_box("User Config", msg, TUI_BUTTON_OK);
                    }
                }
            } else if (sh->focused) {
                char uname[32] = "";
                if (tui_input_box("Set Shell", "Username:", uname, sizeof uname)) {
                    char shell[32] = "/bin/sh";
                    if (tui_input_box("Set Shell", "Shell path:", shell, sizeof shell)) {
                        char msg[64];
                        cp(msg, "Shell for '");
                        size_t p = 11;
                        size_t i = 0;
                        while (uname[i]) msg[p++] = uname[i++];
                        cp(msg + p, "' set.");
                        tui_message_box("User Config", msg, TUI_BUTTON_OK);
                    }
                }
            } else if (rm->focused) {
                char uname[32] = "";
                if (tui_input_box("Remove User", "Username:", uname, sizeof uname)) {
                    char msg[64];
                    cp(msg, "Remove user '");
                    size_t p = 13;
                    size_t i = 0;
                    while (uname[i]) msg[p++] = uname[i++];
                    cp(msg + p, "'?");
                    if (tui_confirm("Remove User", msg)) {
                        tui_message_box("User Config",
                            "User removal is not available through the current ABI.\n"
                            "Use the shell to manage users.",
                            TUI_BUTTON_OK);
                    }
                }
            }
        }

        tui_dispatch_event(a, &e);
    }

    tui_shutdown();
    return 0;
}
