# c2048 for OS64

Bare-metal OS64 port of Maurits van der Schee's MIT-licensed
[`2048.c`](https://github.com/mevdschee/2048.c), version 1.0.3. The board and
move model are adapted to the OS64 application ABI. POSIX termios, signals,
`usleep`, and the hosted C runtime were replaced with OS64 terminal cells and
key events.
