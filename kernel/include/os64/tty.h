#ifndef OS64_TTY_H
#define OS64_TTY_H
#include <stddef.h>
#define TTY_LINE_LENGTH 80
#define TTY_SCROLLBACK 64
typedef struct tty{
 char input[TTY_LINE_LENGTH];size_t length,cursor;
 char lines[TTY_SCROLLBACK][TTY_LINE_LENGTH];size_t line_count;
}tty_t;
void tty_init(tty_t *tty);
void tty_clear(tty_t *tty);
void tty_write_line(tty_t *tty,const char *text);
void tty_write_char(tty_t *tty,char character);
int tty_input_key(tty_t *tty,unsigned key,char *command,size_t capacity);
#endif
