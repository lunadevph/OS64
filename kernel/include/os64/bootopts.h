#ifndef OS64_BOOTOPTS_H
#define OS64_BOOTOPTS_H
#include <stdint.h>
void bootopts_init(uint64_t multiboot);
const char *bootopts_command_line(void);
const char *bootopts_mode(void);
int bootopts_debug(void);
int bootopts_recovery(void);
int bootopts_cli(void);
int bootopts_serial(void);
const void *bootopts_initrd_start(void);
const void *bootopts_initrd_end(void);
#endif
