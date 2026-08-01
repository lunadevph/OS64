#ifndef OS64_TIMED_H
#define OS64_TIMED_H
#include "rtc.h"
#include <stdint.h>
void timed_init(void);
void timed_start(void);
void timed_stop(void);
void timed_poll(void);
int timed_now(rtc_time_t *time);
int timed_sync(void);
int timed_get_realtime_ns(int64_t *nanoseconds);
uint64_t timed_monotonic_ns(void);
int timed_set_server(const char *hostname);
int timed_set_enabled(int enabled);
int timed_enabled(void);
int timed_synchronized(void);
const char *timed_server(void);
const char *timed_timezone(void);
unsigned long timed_updates(void);
unsigned long timed_uptime(void);
unsigned long timed_failures(void);
uint32_t timed_last_address(void);
uint8_t timed_last_stratum(void);
int64_t timed_last_offset_ns(void);
int64_t timed_last_delay_ns(void);
uint64_t timed_last_sync_epoch(void);
int timed_last_error(void);
#endif
