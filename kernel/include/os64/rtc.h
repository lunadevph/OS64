#ifndef OS64_RTC_H
#define OS64_RTC_H
#include <stdint.h>
typedef struct{uint16_t year;uint8_t month,day,hour,minute,second;}rtc_time_t;
int rtc_read(rtc_time_t *time);
#endif
