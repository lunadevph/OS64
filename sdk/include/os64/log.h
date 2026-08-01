#ifndef OS64_SDK_LOG_H
#define OS64_SDK_LOG_H
enum os_log_level{OS_LOG_DEBUG,OS_LOG_INFO,OS_LOG_WARNING,OS_LOG_ERROR};void os_log(enum os_log_level level,const char *message);
#endif
