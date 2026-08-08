#ifndef OS64_SHELL_H
#define OS64_SHELL_H
int kernel_shell_execute(char *line);
const char *kernel_shell_user(void);
const char *kernel_shell_cwd(void);
#endif
