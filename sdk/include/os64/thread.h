#ifndef OS64_SDK_THREAD_H
#define OS64_SDK_THREAD_H
typedef int(*os_thread_entry)(void *context);int os_thread_create(os_thread_entry entry,void *context);int os_thread_join(int thread,int *status);void os_thread_yield(void);
#endif
