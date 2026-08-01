# OS64 classic text UI

`user/libtui` is the shared full-screen text interface library. Applications
acquire the terminal through ABI v3, save the original 80x25 screen, draw into
persistent front/back cell buffers, and restore the shell screen and cursor on
exit. The renderer emits only changed cells. VGA uses native CP437 bytes;
`tui_use_ascii(1)` selects portable borders.

The DOS theme is centralized in `struct tui_theme`. Windows clip through the
screen renderer, can overlap, and modal windows use double borders. Widgets
include labels, buttons, text/password inputs, check/radio controls, lists,
tables, menus, status/progress/tab controls, text views, and scrollbars.
Keyboard input supports Tab, Shift-Tab, arrows, navigation keys, F1-F10,
Escape, Enter, Space and Ctrl-C. Text boxes provide bounded insert/overwrite
editing, deletion, horizontal scrolling, password masking, and the hardware
full-block blinking cursor.

Applications:

- `sysmgr`: live service, memory, storage, and network status; F5 refreshes.
- `fileman`: two-panel `/home` and `/mnt` browser with text viewing.
- `netcfg`, `usercfg`, and `setup`: shared-widget configuration front ends.

Build with `make`. The binaries are installed once into
`build/rootfs/usr/bin` and included in the initrd and ISO. Run the image with
`make run` or `make run-console`, then launch an application by name.

The renderer rejects terminals below 40x15 and checks every cell against screen
bounds. The kernel also releases terminal ownership after every ELF return,
providing a final restoration guard if an application forgets to shut down.
