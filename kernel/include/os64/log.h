#ifndef OS64_LOG_H
#define OS64_LOG_H
#include <stddef.h>
#include <stdint.h>

#define LOG_DEBUG 0
#define LOG_INFO  1
#define LOG_WARN  2
#define LOG_ERROR 3
#define LOG_MAX_ENTRIES 64
#define LOG_MSG_SIZE 56

void log_init(void);
void log_submit(int level, const char *subsystem, const char *msg);
int log_read(size_t index, uint64_t *timestamp, int *level, const char **subsystem, const char **msg);
size_t log_count(void);
int log_level(void);
void log_set_level(int level);
void log_poll(void);
void log_dump(void);

#endif
