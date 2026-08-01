#ifndef OS64_APP_ABI_H
#define OS64_APP_ABI_H
#define OS64_ABI_VERSION 6ul
typedef unsigned long os64_size_t;
typedef struct{unsigned short year;unsigned char month,day,hour,minute,second;}os64_datetime_t;
typedef struct{char name[96];unsigned char type;unsigned char backend;}os64_dirent_t;
typedef struct os64_api{
 unsigned long version;
 int (*dispatch)(const char *name,const char *args);
 void (*write)(const char *text);
 void (*putc)(char value);
 int (*read_file)(const char *path,unsigned char *data,os64_size_t capacity,os64_size_t *size);
 int (*write_file)(const char *path,const unsigned char *data,os64_size_t size);
 void *(*allocate)(os64_size_t size);
 int (*clock_get)(os64_datetime_t *time);
 const char *(*current_user)(void);
 const char *(*current_directory)(void);
 int (*terminal_acquire)(void);
 void (*terminal_release)(void);
 unsigned int (*terminal_read_key)(void);
 void (*terminal_write_cell)(unsigned int x,unsigned int y,unsigned int character,unsigned char attribute);
 void (*terminal_cursor)(unsigned int x,unsigned int y,int visible,int full_block);
 unsigned int (*terminal_width)(void);
 unsigned int (*terminal_height)(void);
 int (*read_directory)(const char *path,unsigned int index,os64_dirent_t *entry);
 unsigned long (*system_query)(const char *name);
 unsigned int (*terminal_poll_key)(void);
 unsigned int (*getuid)(void);
 unsigned int (*getgid)(void);
 int (*setuid)(unsigned int uid);
 int (*setgid)(unsigned int gid);
 int (*check_permission)(const char *path,unsigned int access);
 unsigned int (*get_groups)(void);
 int (*is_admin)(void);
 int (*is_root)(void);
}os64_api_t;
typedef int (*os64_entry_t)(const os64_api_t *api,const char *args);
#endif
