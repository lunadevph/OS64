#ifndef OS64_SECURITY_H
#define OS64_SECURITY_H
#include <stddef.h>
#define OS_ACCESS_EXECUTE 1u
#define OS_ACCESS_WRITE 2u
#define OS_ACCESS_READ 4u
unsigned os_getuid(void);unsigned os_getgid(void);
int os_setuid(unsigned uid);int os_setgid(unsigned gid);
int os_check_permission(const char *path,unsigned access);
int os_is_admin(void);int os_is_root(void);
size_t os_get_groups(unsigned *groups,size_t capacity);
#endif
