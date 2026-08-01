#ifndef OS64_SERVICE_H
#define OS64_SERVICE_H
#include <stddef.h>
typedef enum{SERVICE_STOPPED,SERVICE_STARTING,SERVICE_READY,SERVICE_FAILED}service_state_t;
void service_init(void);
void service_poll_all(void);
int service_find(const char *name);
int service_start(int id);
int service_stop(int id);
int service_wait_ready(int id,unsigned polls);
int service_running(int id);
int service_ready(int id);
service_state_t service_state(int id);
const char *service_state_name(int id);
int service_exit_code(int id);
unsigned long service_generation(int id);
unsigned long service_cycles(int id);
unsigned long service_errors(int id);
unsigned long service_metric(int id);
const char *service_description(int id);
const char *service_name(int id);
size_t service_count(void);
#endif
