#include "service.h"
#include "timed.h"
#include "disk.h"
#include "varfs.h"
#include "memory.h"
#include "auth.h"
#include "fs.h"
#include "network.h"
#include "display.h"
#include "graphics.h"
#include "log.h"

typedef struct {
    const char *name;
    const char *description;
    int dependency;
    service_state_t state;
    int exit_code;
    unsigned long generation;
    unsigned long cycles;
    unsigned long errors;
    unsigned long metric;
} service_t;

static service_t services[] = {
    {"fsd",     "VFS and mount coordinator",       -1, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"memoryd", "memory accounting service",       -1, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"timed",   "CMOS real-time clock service",     1, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"diskd",   "disk synchronization service",     0, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"userd",   "account database service",         3, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"acpid",   "ACPI power and reboot service",    2, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"netd",    "PCI Ethernet, ARP, IPv4 and ICMP", 3, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"displayd", "console display service",          1, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"graphicsd","VGA framebuffer compositor",       7, SERVICE_STOPPED, 0, 0, 0, 0, 0},
    {"logd",    "kernel message logger",             2, SERVICE_STOPPED, 0, 0, 0, 0, 0}
};

static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
size_t service_count(void){return sizeof services/sizeof services[0];}

void service_init(void){
    for(size_t i=0;i<service_count();i++){
        services[i].state=SERVICE_STOPPED;
        services[i].exit_code=0;
        services[i].generation=services[i].cycles=services[i].errors=services[i].metric=0;
    }
    timed_init();
    timed_stop();
}

static void fail(service_t*s,int code){s->state=SERVICE_FAILED;s->exit_code=code;s->errors++;}

static void poll_one(size_t i){
    service_t*s=&services[i];
    if(s->state!=SERVICE_STARTING&&s->state!=SERVICE_READY)return;
    s->cycles++;
    if(i==0){
        s->metric=(unsigned long)fs_count();
        if(s->metric)s->state=SERVICE_READY;
    }else if(i==1){
        s->metric=(unsigned long)memory_free_bytes();
        if(memory_heap_bytes())s->state=SERVICE_READY;
        else fail(s,12);
    }else if(i==2){
        timed_poll();
        s->metric=timed_updates();
        if(s->metric)s->state=SERVICE_READY;
    }else if(i==3){
        if(s->state==SERVICE_STARTING){if(!disk_sync())fail(s,5);else{s->metric++;s->state=SERVICE_READY;}}
        else s->metric++;
    }else if(i==4){
        s->metric=(unsigned long)auth_user_count();
        if(s->metric)s->state=SERVICE_READY;
        else fail(s,2);
    }else if(i==5){
        s->metric=1;
        s->state=SERVICE_READY;
    }else if(i==6){
        network_poll();
        s->metric=network_rx_packets()+network_tx_packets();
        if(network_ready())s->state=SERVICE_READY;
        else fail(s,19);
    }else if(i==7){
        s->metric=display_characters();
        if(display_ready())s->state=SERVICE_READY;
        else fail(s,6);
    }else if(i==9){
        log_poll();
        s->metric=(unsigned long)log_count();
        if(log_count())s->state=SERVICE_READY;
        else{s->state=SERVICE_READY;s->metric=1;}
    }else{
        graphics_poll();
        s->metric=graphics_frames();
        if(graphics_ready())s->state=SERVICE_READY;
        else fail(s,6);
    }
}

void service_poll_all(void){for(size_t i=0;i<service_count();i++)poll_one(i);}
int service_find(const char*n){for(size_t i=0;i<service_count();i++)if(eq(n,services[i].name))return (int)i;return -1;}

int service_start(int id){
    if(id<0||(size_t)id>=service_count())return 0;
    service_t*s=&services[id];
    if(s->state==SERVICE_READY||s->state==SERVICE_STARTING)return 1;
    if(s->dependency>=0&&services[s->dependency].state!=SERVICE_READY){
        s->state=SERVICE_FAILED;
        s->exit_code=69;
        s->errors++;
        return 0;
    }
    s->state=SERVICE_STARTING;
    s->exit_code=0;
    s->generation++;
    if(id==2)timed_start();
    if(id==6&&network_init()==0){s->state=SERVICE_FAILED;s->exit_code=19;s->errors++;return 0;}
    if(id==8&&graphics_init()==0){s->state=SERVICE_FAILED;s->exit_code=6;s->errors++;return 0;}
    if(id==9)log_init();
    return 1;
}

int service_stop(int id){
    if(id<0||(size_t)id>=service_count())return 0;
    services[id].state=SERVICE_STOPPED;
    services[id].exit_code=0;
    if(id==2)timed_stop();
    if(id==8)graphics_stop();
    return 1;
}

int service_wait_ready(int id,unsigned polls){
    if(!service_start(id))return 0;
    while(polls--&&services[id].state==SERVICE_STARTING)poll_one((size_t)id);
    if(services[id].state==SERVICE_STARTING)fail(&services[id],110);
    return services[id].state==SERVICE_READY;
}

int service_running(int id){return id>=0&&(size_t)id<service_count()&&(services[id].state==SERVICE_STARTING||services[id].state==SERVICE_READY);}
int service_ready(int id){return id>=0&&(size_t)id<service_count()&&services[id].state==SERVICE_READY;}
service_state_t service_state(int id){return id>=0&&(size_t)id<service_count()?services[id].state:SERVICE_FAILED;}
int service_exit_code(int id){return id>=0&&(size_t)id<service_count()?services[id].exit_code:127;}
const char*service_state_name(int id){service_state_t s=service_state(id);return s==SERVICE_STOPPED?"stopped":s==SERVICE_STARTING?"starting":s==SERVICE_READY?"ready":"failed";}
unsigned long service_generation(int id){return id>=0&&(size_t)id<service_count()?services[id].generation:0;}
unsigned long service_cycles(int id){return id>=0&&(size_t)id<service_count()?services[id].cycles:0;}
unsigned long service_errors(int id){return id>=0&&(size_t)id<service_count()?services[id].errors:0;}
unsigned long service_metric(int id){return id>=0&&(size_t)id<service_count()?services[id].metric:0;}
const char*service_description(int id){return id>=0&&(size_t)id<service_count()?services[id].description:"unknown";}
const char*service_name(int id){return id>=0&&(size_t)id<service_count()?services[id].name:"unknown";}
