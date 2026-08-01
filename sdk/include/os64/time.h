#ifndef OS64_SDK_TIME_H
#define OS64_SDK_TIME_H
#include "app_abi.h"
int os_clock_realtime(os64_datetime_t *time);unsigned long os_timer_monotonic(void);void os_sleep(unsigned milliseconds);
#endif
