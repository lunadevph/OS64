#include "timed.h"
#include "network.h"
#include "fs.h"
#include "varfs.h"
#include <stdint.h>
#include <stddef.h>

#define NTP_EPOCH 2208988800ull
#define NS_SECOND 1000000000ll
#define DEFAULT_SERVER "time.cloudflare.com"

static rtc_time_t clock;
static int running,valid,configured,enabled=1,synchronized,last_error;
static int64_t realtime_correction_ns,last_offset_ns,last_delay_ns;
static uint64_t base_tsc,base_utc_ns,tsc_hz,last_sync_epoch,next_attempt_ns;
static uint32_t last_address;
static uint8_t last_stratum;
static unsigned timeout_ms=3000,retries=3,sync_interval=3600;
static unsigned long updates,uptime,failures;
static char server[96]=DEFAULT_SERVER;

static uint64_t rdtsc(void){uint32_t lo,hi;__asm__ volatile("rdtsc":"=a"(lo),"=d"(hi));return ((uint64_t)hi<<32)|lo;}
static uint64_t cpu_hz(void){
 uint32_t max,a,b,c,d;__asm__ volatile("cpuid":"=a"(max),"=b"(b),"=c"(c),"=d"(d):"a"(0),"c"(0));
 if(max>=0x16){__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(0x16),"c"(0));if(a)return (uint64_t)a*1000000ull;}
 return 1000000000ull;
}
static uint64_t elapsed_ns(uint64_t ticks){uint64_t q=ticks/tsc_hz,r=ticks%tsc_hz;return q*1000000000ull+(r*1000000000ull)/tsc_hz;}
static int leap(unsigned y){return !(y%4)&&((y%100)||(y%400==0));}
static unsigned month_days(unsigned y,unsigned m){static const uint8_t n[]={31,28,31,30,31,30,31,31,30,31,30,31};return n[m-1]+(m==2&&leap(y));}
static uint64_t to_epoch(const rtc_time_t*t){uint64_t days=0;for(unsigned y=1970;y<t->year;y++)days+=leap(y)?366u:365u;for(unsigned m=1;m<t->month;m++)days+=month_days(t->year,m);return (days+t->day-1u)*86400u+(uint64_t)t->hour*3600u+(uint64_t)t->minute*60u+t->second;}
static void from_epoch(uint64_t value,rtc_time_t*t){uint64_t days=value/86400u,s=value%86400u;unsigned y=1970;while(days>=(uint64_t)(leap(y)?366:365))days-=leap(y)?366u:365u,y++;unsigned m=1;while(days>=month_days(y,m))days-=month_days(y,m),m++;t->year=(uint16_t)y;t->month=(uint8_t)m;t->day=(uint8_t)(days+1);t->hour=(uint8_t)(s/3600u);t->minute=(uint8_t)((s%3600u)/60u);t->second=(uint8_t)(s%60u);}
static int same(const rtc_time_t*a,const rtc_time_t*b){return a->year==b->year&&a->month==b->month&&a->day==b->day&&a->hour==b->hour&&a->minute==b->minute&&a->second==b->second;}
static int parse_uint(const char*s,size_t n,unsigned*value){unsigned v=0;if(!n)return 0;for(size_t i=0;i<n;i++){if(s[i]<'0'||s[i]>'9'||v>1000000)return 0;v=v*10u+(unsigned)(s[i]-'0');}*value=v;return 1;}
static int hostname_valid(const char*s){size_t n=0;int label=0;while(s[n]){char c=s[n];if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return 0;if(c=='.'){if(!label)return 0;label=0;}else label++;if(++n>=sizeof server||label>63)return 0;}return n&&label;}
static void set_text(char*d,const char*s,size_t cap){size_t n=0;while(s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static void parse_config(const unsigned char*d,size_t size){
 size_t p=0;while(p<size){while(p<size&&(d[p]==' '||d[p]=='\t'||d[p]=='\r'||d[p]=='\n'))p++;if(p>=size)break;if(d[p]=='#'){while(p<size&&d[p++]!='\n');continue;}size_t k=p;while(p<size&&d[p]!='='&&d[p]!='\n')p++;if(p>=size||d[p]!='='){while(p<size&&d[p++]!='\n');continue;}size_t kn=p++-k,v=p;while(p<size&&d[p]!='\r'&&d[p]!='\n')p++;size_t vn=p-v;unsigned number;
  if(kn==6&&d[k]=='s'&&d[k+1]=='e'&&d[k+2]=='r'&&d[k+3]=='v'&&d[k+4]=='e'&&d[k+5]=='r'&&vn<sizeof server){char h[96];for(size_t i=0;i<vn;i++)h[i]=(char)d[v+i];h[vn]=0;if(hostname_valid(h))set_text(server,h,sizeof server);}
  else if(kn==7&&d[k]=='e'&&d[k+1]=='n'&&d[k+2]=='a'&&d[k+3]=='b'&&d[k+4]=='l'&&d[k+5]=='e'&&d[k+6]=='d')enabled=vn==4&&d[v]=='t'&&d[v+1]=='r'&&d[v+2]=='u'&&d[v+3]=='e';
  else if(kn==10&&d[k]=='t'&&parse_uint((const char*)d+v,vn,&number)&&number>=100&&number<=30000)timeout_ms=number;
  else if(kn==7&&d[k]=='r'&&parse_uint((const char*)d+v,vn,&number)&&number>=1&&number<=5)retries=number;
  else if(kn==13&&d[k]=='s'&&parse_uint((const char*)d+v,vn,&number)&&number>=60&&number<=86400)sync_interval=number;
 }
}
static void load_config(void){if(configured)return;configured=1;fs_file_t f;if(fs_find("etc/ntp.conf",&f))parse_config(f.data,f.size);unsigned char b[512];size_t n;if(varfs_mounted()&&varfs_load("/var/lib/os64/ntp.conf",b,sizeof b,&n))parse_config(b,n);}
static int save_config(void){char b[192];size_t n=0;const char*p="server=";while(*p)b[n++]=*p++;for(size_t i=0;server[i];i++)b[n++]=server[i];p="\nenabled=";while(*p)b[n++]=*p++;p=enabled?"true\n":"false\n";while(*p)b[n++]=*p++;return varfs_mounted()&&varfs_store("/var/lib/os64/ntp.conf",(const unsigned char*)b,n)&&varfs_sync();}
uint64_t timed_monotonic_ns(void){return elapsed_ns(rdtsc()-base_tsc);}
int timed_get_realtime_ns(int64_t*out){if(!running||!out)return 0;*out=(int64_t)(base_utc_ns+timed_monotonic_ns())+realtime_correction_ns;return 1;}
static uint64_t ntp_from_ns(int64_t ns){uint64_t sec=(uint64_t)(ns/NS_SECOND)+NTP_EPOCH,frac=(uint64_t)(ns%NS_SECOND);return (sec<<32)|((frac<<32)/NS_SECOND);}
static int64_t fixed_delta_ns(uint64_t a,uint64_t b){int64_t d=(int64_t)(a-b),q=d/4294967296ll,r=d%4294967296ll;return q*NS_SECOND+(r*NS_SECOND)/4294967296ll;}
void timed_init(void){rtc_time_t raw;tsc_hz=cpu_hz();base_tsc=rdtsc();running=1;valid=configured=synchronized=0;last_error=0;realtime_correction_ns=last_offset_ns=last_delay_ns=0;updates=uptime=failures=0;last_sync_epoch=next_attempt_ns=0;if(rtc_read(&raw)){base_utc_ns=to_epoch(&raw)*1000000000ull;valid=1;clock=raw;updates=1;}else base_utc_ns=0;}
void timed_start(void){running=1;load_config();if(enabled&&!next_attempt_ns)next_attempt_ns=timed_monotonic_ns()+1000000000ull;}
void timed_stop(void){running=0;}
int timed_sync(void){
 load_config();if(!running||!network_ready()){last_error=NETWORK_NTP_DOWN;failures++;return last_error;}
 int64_t t1ns;if(!timed_get_realtime_ns(&t1ns))return NETWORK_NTP_TIMESTAMP;uint64_t t1=ntp_from_ns(t1ns);network_ntp_result_t reply;int result=NETWORK_NTP_TIMEOUT;
 for(unsigned i=0;i<retries;i++){result=network_ntp_query(server,t1,&reply,timeout_ms*1400u);if(result==NETWORK_NTP_OK)break;}
 if(result!=NETWORK_NTP_OK){last_error=result;failures++;next_attempt_ns=timed_monotonic_ns()+30000000000ull;return result;}
 int64_t t4ns=0;if(!timed_get_realtime_ns(&t4ns)){last_error=NETWORK_NTP_TIMESTAMP;failures++;return last_error;}uint64_t t4=ntp_from_ns(t4ns);
 int64_t a=fixed_delta_ns(reply.receive_timestamp,t1),b=fixed_delta_ns(reply.transmit_timestamp,t4);
 int64_t server=fixed_delta_ns(reply.transmit_timestamp,reply.receive_timestamp);
 int64_t local=fixed_delta_ns(t4,t1);last_offset_ns=(a+b)/2;last_delay_ns=local-server;if(last_delay_ns<0||last_delay_ns>30000000000ll){last_error=NETWORK_NTP_TIMESTAMP;failures++;return last_error;}
 realtime_correction_ns+=last_offset_ns;synchronized=1;last_error=0;last_address=reply.address;last_stratum=reply.stratum;last_sync_epoch=(uint64_t)((t4ns+last_offset_ns)/NS_SECOND);next_attempt_ns=timed_monotonic_ns()+(uint64_t)sync_interval*1000000000ull;updates++;return 1;
}
void timed_poll(void){if(!running)return;load_config();int64_t utc;if(timed_get_realtime_ns(&utc)&&utc>=0){rtc_time_t sample;from_epoch((uint64_t)(utc/NS_SECOND)+8u*3600u,&sample);if(!valid||!same(&clock,&sample)){if(valid)uptime++;clock=sample;valid=1;updates++;}}if(enabled&&network_ready()&&next_attempt_ns&&timed_monotonic_ns()>=next_attempt_ns)timed_sync();}
int timed_now(rtc_time_t*t){if(!running||!t)return 0;timed_poll();if(!valid)return 0;*t=clock;return 1;}
int timed_set_server(const char*h){load_config();if(!hostname_valid(h))return 0;set_text(server,h,sizeof server);synchronized=0;return save_config();}
int timed_set_enabled(int value){load_config();enabled=value!=0;if(enabled)next_attempt_ns=timed_monotonic_ns()+1000000000ull;return save_config();}
int timed_enabled(void){load_config();return enabled;}int timed_synchronized(void){return synchronized;}
const char*timed_server(void){load_config();return server;}const char*timed_timezone(void){return "GMT+8";}
uint32_t timed_last_address(void){return last_address;}uint8_t timed_last_stratum(void){return last_stratum;}
int64_t timed_last_offset_ns(void){return last_offset_ns;}int64_t timed_last_delay_ns(void){return last_delay_ns;}
uint64_t timed_last_sync_epoch(void){return last_sync_epoch;}int timed_last_error(void){return last_error;}
unsigned long timed_failures(void){return failures;}unsigned long timed_updates(void){return updates;}unsigned long timed_uptime(void){return uptime;}
