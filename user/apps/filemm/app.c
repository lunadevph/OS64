#include "tui.h"

static void cp(char *d, const char *s, size_t cap) {
    size_t n = 0;
    while (s[n] && n + 1 < cap) { d[n] = s[n]; n++; }
    d[n] = 0;
}

struct pane {
    char path[96];
    char names[TUI_MAX_ITEMS][96];
    const char *items[TUI_MAX_ITEMS];
    size_t count;
    struct tui_widget *list;
};

static int load_dir(const os64_api_t *a, struct pane *p) {
    os64_dirent_t e;
    size_t n = 0;
    cp(p->names[n], "..", 96);
    p->items[n++] = p->names[0];
    for (unsigned i = 0; n < TUI_MAX_ITEMS && a->read_directory(p->path, i, &e); i++) {
        cp(p->names[n], e.name, 96);
        p->items[n] = p->names[n];
        n++;
    }
    p->count = n;
    p->list->item_count = n;
    p->list->selected = 0;
    for (size_t i = 0; i < n; i++)
        p->list->items[i] = p->items[i];
    return n > 0;
}

static void path_join(char *out, size_t cap, const char *base, const char *name) {
    cp(out, base, cap);
    size_t p = 0;
    while (out[p]) p++;
    out[p++] = '/';
    cp(out + p, name, cap - p);
}

int _start(const os64_api_t *api, const char *args) {
    (void)args;
    if (!tui_initialize(api)) {
        api->write("filemm: terminal must be at least 40x15\n");
        return 1;
    }

    struct tui_application *a = tui_app_create("filemm");
    struct tui_window *w = tui_window_create(0, 0, 80, 25, "OS64 File Manager", 0);

    static struct pane left, right;
    cp(left.path, "/home", sizeof left.path);
    cp(right.path, "/mnt", sizeof right.path);

    left.list = tui_listbox_create(w, 1, 2, 36, 17, left.items, 0);
    right.list = tui_listbox_create(w, 39, 2, 36, 17, right.items, 0);
    tui_widget_set_focus(left.list);
    left.list->item_count = 0;
    right.list->item_count = 0;

    load_dir(api, &left);
    load_dir(api, &right);

    struct tui_widget *path_w = tui_widget_create(TUI_STATUS_BAR, w, 1, 20, 76, 1, "");

    tui_widget_create(TUI_STATUS_BAR, w, 1, 22, 76, 1,
        "F1 Help  F3 View  Tab Switch  F5 Copy  F6 Move  F7 MkDir  F8 Delete  F10 Exit");

    for (;;) {
        struct pane *active = left.list->focused ? &left : &right;

        size_t pp = 0;
        while (active->path[pp]) pp++;
        const char *pfix = "Path: ";
        size_t pn = 0;
        while (pfix[pn]) pn++;
        char pb[160];
        cp(pb, pfix, sizeof pb);
        cp(pb + pn, active->path, sizeof pb - pn);
        path_w->width = 76;
        cp(path_w->text, pb, sizeof pb);

        tui_render();
        struct tui_event e;
        tui_next_event(&e);
        if (e.key == TUI_KEY_F10 || e.key == 27 || e.type == TUI_EVENT_CLOSE)
            break;

        if (e.key == TUI_KEY_F1) {
            tui_message_box("FileMM Help",
                "Tab switches the active panel.\n"
                "Enter opens the selected directory.\n"
                "Backspace goes to the parent directory.\n"
                "F3 views a text file.\n"
                "F7 prompts for a new directory.\n"
                "F10 or Esc exits.",
                TUI_BUTTON_OK);
        } else if (e.key == '\n' && active->list->selected > 0) {
            char new_path[96];
            path_join(new_path, sizeof new_path, active->path,
                      active->items[active->list->selected]);
            char saved[96];
            cp(saved, active->path, sizeof saved);
            cp(active->path, new_path, sizeof active->path);
            if (!load_dir(api, active)) {
                cp(active->path, saved, sizeof active->path);
                // try viewing as file
                os64_size_t n;
                char data[512];
                if (api->read_file(new_path, (unsigned char *)data, sizeof data - 1, &n)) {
                    data[n] = 0;
                    tui_message_box(active->items[active->list->selected], data, TUI_BUTTON_OK);
                }
            }
        } else if (e.key == '\b') {
            // parent directory
            size_t p = 0;
            while (active->path[p]) p++;
            while (p > 0 && active->path[p] != '/') p--;
            if (p == 0) { active->path[1] = 0; }
            else if (p > 0) { active->path[p] = 0; }
            load_dir(api, active);
        } else if (e.key == 0x83) { // F3 View
            if (active->list->selected > 0) {
                char fp[160];
                path_join(fp, sizeof fp, active->path,
                          active->items[active->list->selected]);
                os64_size_t n;
                char data[512];
                if (api->read_file(fp, (unsigned char *)data, sizeof data - 1, &n)) {
                    data[n] = 0;
                    tui_message_box(active->items[active->list->selected], data, TUI_BUTTON_OK);
                } else {
                    tui_message_box("Error", "Could not open file.", TUI_BUTTON_OK);
                }
            }
        } else if (e.key == 0x85) { // F5 Copy
            tui_message_box("Copy",
                "Copy is not supported through the current ABI.\n"
                "Use 'cp <source> <dest>' from the shell.",
                TUI_BUTTON_OK);
        } else if (e.key == 0x86) { // F6 Move
            tui_message_box("Move",
                "Move is not supported through the current ABI.\n"
                "Use 'mv <source> <dest>' from the shell.",
                TUI_BUTTON_OK);
        } else if (e.key == 0x87) { // F7 MkDir
            char dirname[64] = "";
            if (tui_input_box("Create Directory", "Name:", dirname, sizeof dirname)) {
                tui_message_box("MkDir",
                    "Use 'mkdir <path>' from the shell to create a directory.",
                    TUI_BUTTON_OK);
            }
        } else if (e.key == 0x88) { // F8 Delete
            if (active->list->selected > 0) {
                char msg[128];
                cp(msg, "Delete '", sizeof msg);
                size_t ml = 8;
                const char *fn = active->items[active->list->selected];
                while (*fn && ml < sizeof msg - 2) msg[ml++] = *fn++;
                msg[ml++] = '\'';
                msg[ml++] = '?';
                msg[ml] = 0;
                if (tui_confirm("Delete", msg)) {
                    tui_message_box("Delete",
                        "Use 'rm <path>' from the shell to delete files.",
                        TUI_BUTTON_OK);
                }
            }
        } else {
            tui_dispatch_event(a, &e);
        }
    }

    tui_shutdown();
    return 0;
}
