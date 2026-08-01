#include "tls_platform.h"
#include "network.h"
#include "timed.h"

static int leap(uint32_t y){return (!(y%4)&&y%100)||!(y%400);}
int tls_socket_connect(tls_socket_t*s,const char*host,uint16_t port){
    if(!s||!host)return TLS_PLATFORM_DNS;
    uint32_t address;
    if(network_resolve(host,&address,8000000)!=1)return TLS_PLATFORM_DNS;
    if(network_tcp_connect(address,port,8000000)!=1)return TLS_PLATFORM_TCP;
    s->connected=1;return TLS_PLATFORM_OK;
}
int tls_socket_send(tls_socket_t*s,const void*d,size_t n){return s&&s->connected&&network_tcp_send(d,n,8000000)==1?TLS_PLATFORM_OK:TLS_PLATFORM_TCP;}
int tls_socket_receive(tls_socket_t*s,void*d,size_t c,size_t*n){return s&&s->connected&&network_tcp_receive(d,c,n,64000000)>=0?TLS_PLATFORM_OK:TLS_PLATFORM_TCP;}
void tls_socket_close(tls_socket_t*s){if(s&&s->connected)network_tcp_close(100000);if(s)s->connected=0;}
int tls_entropy(void*output,size_t length){
    uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0));
    if(!(c&(1u<<30)))return TLS_PLATFORM_ENTROPY;
    uint8_t*out=(uint8_t*)output;while(length){uint64_t value;unsigned char ok;
        do{__asm__ volatile("rdrand %0; setc %1":"=r"(value),"=qm"(ok));}while(!ok);
        for(unsigned i=0;i<8&&length;i++,length--)*out++=(uint8_t)(value>>(i*8));
    }return TLS_PLATFORM_OK;
}
int tls_validation_time(uint32_t*days,uint32_t*seconds){
    rtc_time_t t;if(!days||!seconds||!timed_now(&t)||t.year<2024)return TLS_PLATFORM_CLOCK;
    static const uint16_t before[]={0,31,59,90,120,151,181,212,243,273,304,334};
    uint32_t y=t.year,total=365*y+(y+3)/4-(y+99)/100+(y+399)/400;
    total+=before[t.month-1]+t.day-1;if(t.month>2&&leap(y))total++;
    *days=total;*seconds=(uint32_t)t.hour*3600u+(uint32_t)t.minute*60u+t.second;return TLS_PLATFORM_OK;
}
