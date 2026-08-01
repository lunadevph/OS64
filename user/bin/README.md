# Normal commands

Normal command names are declared in `../commands.conf`. The user Makefile
builds their ELF command trampolines from `../lib/builtin.c` and installs them
directly into the staged `/bin` directory.
