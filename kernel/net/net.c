#include "network.h"
#include "process.h"
#include "netdev.h"
#include <stddef.h>

static const uint32_t ip=0x0f02000au;
static const uint32_t gateway=0x0202000au;
static const uint32_t dns_server=0x0302000au;
static const uint32_t netmask=0x00ffffffu;

typedef struct{uint32_t ip;uint8_t mac[6];int valid;} neighbor_t;
static neighbor_t neighbors[8];
static int ready;
static uint16_t ip_id,sequence;
static volatile uint16_t reply_sequence;
static volatile int ping_error;
static volatile uint8_t ping_error_type,ping_error_code;
static volatile uint16_t dns_reply_id;
static volatile int dns_reply_status;
static volatile uint32_t dns_reply_address;
static volatile int ntp_reply_status;
static uint32_t ntp_server_ip;
static uint64_t ntp_request_timestamp;
static network_ntp_result_t ntp_reply;

static uint32_t tcp_remote_ip,tcp_local_seq,tcp_remote_seq;
static volatile uint32_t tcp_last_ack;
static uint16_t tcp_local_port,tcp_remote_port;
static volatile int tcp_state;
static uint8_t tcp_queue[32768];
static volatile size_t tcp_queue_length;
typedef struct{uint32_t sequence;uint16_t length;uint8_t fin,valid;uint8_t data[1460];}tcp_fragment_t;
static tcp_fragment_t tcp_fragments[8];

static void copy(uint8_t*d,const uint8_t*s,size_t n){for(size_t i=0;i<n;i++)d[i]=s[i];}
static uint16_t read16(const uint8_t*p){return (uint16_t)((p[0]<<8)|p[1]);}
static uint32_t read32(const uint8_t*p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static uint64_t read64(const uint8_t*p){return ((uint64_t)read32(p)<<32)|read32(p+4);}
static void write16(uint8_t*p,uint16_t v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void write32(uint8_t*p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void write64(uint8_t*p,uint64_t v){write32(p,(uint32_t)(v>>32));write32(p+4,(uint32_t)v);}
static uint16_t checksum(const uint8_t*d,size_t n){uint32_t sum=0;while(n>1){sum+=read16(d);d+=2;n-=2;}if(n)sum+=(uint16_t)(d[0]<<8);while(sum>>16)sum=(sum&0xffff)+(sum>>16);return (uint16_t)~sum;}
static void ethernet(uint8_t*f,const uint8_t*dst,uint16_t type){copy(f,dst,6);copy(f+6,netdev_mac(),6);write16(f+12,type);}
static uint32_t next_hop(uint32_t destination){return (destination&netmask)==(ip&netmask)?destination:gateway;}
static neighbor_t*neighbor_find(uint32_t address){for(unsigned i=0;i<8;i++)if(neighbors[i].valid&&neighbors[i].ip==address)return &neighbors[i];return 0;}
static void neighbor_store(uint32_t address,const uint8_t*mac){neighbor_t*n=neighbor_find(address);if(!n){for(unsigned i=0;i<8;i++)if(!neighbors[i].valid){n=&neighbors[i];break;}}if(!n)n=&neighbors[address&7u];n->ip=address;copy(n->mac,mac,6);n->valid=1;}

static void arp_request(uint32_t target){
    uint8_t f[42],broadcast[6]={255,255,255,255,255,255};ethernet(f,broadcast,0x0806);
    write16(f+14,1);write16(f+16,0x0800);f[18]=6;f[19]=4;write16(f+20,1);
    copy(f+22,netdev_mac(),6);copy(f+28,(const uint8_t*)&ip,4);
    for(unsigned i=0;i<6;i++)f[32+i]=0;
    copy(f+38,(const uint8_t*)&target,4);netdev_send(f,sizeof f);
}
static void arp_reply(const uint8_t*r){
    uint8_t f[42];ethernet(f,r+22,0x0806);copy(f+14,r+14,8);write16(f+20,2);
    copy(f+22,netdev_mac(),6);copy(f+28,(const uint8_t*)&ip,4);copy(f+32,r+22,6);copy(f+38,r+28,4);netdev_send(f,sizeof f);
}
static neighbor_t*resolve_next_hop(uint32_t destination,unsigned spins);

static size_t ipv4_begin(uint8_t*f,uint32_t destination,uint8_t protocol,size_t payload,const uint8_t*mac){
    ethernet(f,mac,0x0800);f[14]=0x45;f[15]=0;write16(f+16,(uint16_t)(20+payload));
    write16(f+18,++ip_id);write16(f+20,0);f[22]=64;f[23]=protocol;write16(f+24,0);
    copy(f+26,(const uint8_t*)&ip,4);copy(f+30,(const uint8_t*)&destination,4);
    write16(f+24,checksum(f+14,20));return 34;
}
static void icmp_reply(const uint8_t*r,size_t n){
    if(n<42)return;
    uint8_t f[1514];copy(f,r,n);copy(f,r+6,6);copy(f+6,netdev_mac(),6);
    copy(f+26,r+30,4);copy(f+30,(const uint8_t*)&ip,4);f[34]=0;write16(f+36,0);
    write16(f+36,checksum(f+34,n-34));netdev_send(f,n);
}

static size_t dns_skip_name(const uint8_t*d,size_t n,size_t p){
    unsigned labels=0;while(p<n&&labels++<128){uint8_t z=d[p++];if((z&0xc0)==0xc0)return p<n?p+1:0;if(!z)return p;if(z>63||p+z>n)return 0;p+=z;}return 0;
}
static void dns_receive(const uint8_t*d,size_t n){
    if(n<12)return;
    uint16_t id=read16(d),flags=read16(d+2),questions=read16(d+4),answers=read16(d+6);
    if(id!=dns_reply_id||!(flags&0x8000))return;
    if((flags&15)!=0){dns_reply_status=-(int)(flags&15);return;}
    size_t p=12;for(unsigned i=0;i<questions;i++){p=dns_skip_name(d,n,p);if(!p||p+4>n){dns_reply_status=-10;return;}p+=4;}
    for(unsigned i=0;i<answers;i++){p=dns_skip_name(d,n,p);if(!p||p+10>n){dns_reply_status=-10;return;}uint16_t type=read16(d+p),class_code=read16(d+p+2),length=read16(d+p+8);p+=10;if(p+length>n){dns_reply_status=-10;return;}if(type==1&&class_code==1&&length==4){uint32_t a;copy((uint8_t*)&a,d+p,4);dns_reply_address=a;dns_reply_status=1;return;}p+=length;}dns_reply_status=-4;
}
static void ntp_receive(const uint8_t*d,size_t n,uint32_t source){
    if(source!=ntp_server_ip)return;
    if(n<48){ntp_reply_status=NETWORK_NTP_SHORT;return;}
    uint8_t leap=d[0]>>6,mode=d[0]&7u,stratum=d[1];
    if(mode!=4){ntp_reply_status=NETWORK_NTP_MODE;return;}
    if(leap==3){ntp_reply_status=NETWORK_NTP_LEAP;return;}
    if(!stratum||stratum>=16){ntp_reply_status=NETWORK_NTP_STRATUM;return;}
    uint64_t originate=read64(d+24),receive=read64(d+32),transmit=read64(d+40);
    if(originate!=ntp_request_timestamp){ntp_reply_status=NETWORK_NTP_ORIGIN;return;}
    if(!receive||!transmit||transmit<receive){ntp_reply_status=NETWORK_NTP_TIMESTAMP;return;}
    ntp_reply.address=source;ntp_reply.stratum=stratum;
    ntp_reply.receive_timestamp=receive;ntp_reply.transmit_timestamp=transmit;
    ntp_reply_status=NETWORK_NTP_OK;
}

static uint16_t tcp_checksum(uint32_t destination,const uint8_t*tcp,size_t n){
    uint8_t pseudo[1480];copy(pseudo,(const uint8_t*)&ip,4);copy(pseudo+4,(const uint8_t*)&destination,4);
    pseudo[8]=0;pseudo[9]=6;write16(pseudo+10,(uint16_t)n);copy(pseudo+12,tcp,n);return checksum(pseudo,n+12);
}
static int tcp_send(uint8_t flags,const uint8_t*data,size_t length){
    neighbor_t*n=resolve_next_hop(tcp_remote_ip,2000000);if(!n||length>1400)return 0;
    uint8_t f[1514];size_t p=ipv4_begin(f,tcp_remote_ip,6,20+length,n->mac);
    write16(f+p,tcp_local_port);write16(f+p+2,tcp_remote_port);write32(f+p+4,tcp_local_seq);write32(f+p+8,tcp_remote_seq);
    f[p+12]=0x50;f[p+13]=flags;write16(f+p+14,4096);write16(f+p+16,0);write16(f+p+18,0);
    if(length)copy(f+p+20,data,length);
    write16(f+p+16,tcp_checksum(tcp_remote_ip,f+p,20+length));
    return netdev_send(f,p+20+length);
}
static void tcp_append(const uint8_t*data,size_t length){
    size_t take=length;if(take>sizeof tcp_queue-tcp_queue_length)take=sizeof tcp_queue-tcp_queue_length;
    for(size_t i=0;i<take;i++)tcp_queue[tcp_queue_length+i]=data[i];
    tcp_queue_length+=take;tcp_remote_seq+=(uint32_t)length;
}
static void tcp_store_fragment(uint32_t seq,const uint8_t*data,size_t length,int fin){
    if(length>1460)return;
    for(unsigned i=0;i<8;i++)if(tcp_fragments[i].valid&&tcp_fragments[i].sequence==seq)return;
    for(unsigned i=0;i<8;i++)if(!tcp_fragments[i].valid){tcp_fragments[i].sequence=seq;tcp_fragments[i].length=(uint16_t)length;tcp_fragments[i].fin=(uint8_t)fin;tcp_fragments[i].valid=1;for(size_t j=0;j<length;j++)tcp_fragments[i].data[j]=data[j];return;}
}
static void tcp_drain_fragments(void){
    int progress=1;while(progress){progress=0;for(unsigned i=0;i<8;i++)if(tcp_fragments[i].valid&&tcp_fragments[i].sequence==tcp_remote_seq){tcp_append(tcp_fragments[i].data,tcp_fragments[i].length);if(tcp_fragments[i].fin){tcp_remote_seq++;tcp_state=4;}tcp_fragments[i].valid=0;progress=1;break;}}
}
static void tcp_receive(const uint8_t*tcp,size_t n,uint32_t source){
    if(source!=tcp_remote_ip||n<20||read16(tcp+2)!=tcp_local_port||read16(tcp)!=tcp_remote_port)return;
    size_t header=(size_t)(tcp[12]>>4)*4;if(header<20||header>n)return;uint8_t flags=tcp[13];
    uint32_t seq=read32(tcp+4),ack=read32(tcp+8);if(flags&4){tcp_state=-2;return;}
    if(flags&0x10)tcp_last_ack=ack;
    if((flags&0x12)==0x12&&tcp_state==1){tcp_remote_seq=seq+1;tcp_local_seq=ack;tcp_last_ack=ack;tcp_state=2;return;}
    size_t payload=n-header;
    if((payload||(flags&1))&&seq!=tcp_remote_seq){if((int32_t)(seq-tcp_remote_seq)>0)tcp_store_fragment(seq,tcp+header,payload,flags&1);tcp_send(0x10,0,0);return;}
    if(payload){
        tcp_append(tcp+header,payload);
    }
    if(flags&1){tcp_remote_seq++;tcp_state=4;}
    tcp_drain_fragments();tcp_send(0x10,0,0);
}

static void process(const uint8_t*f,size_t n){
    if(n<14)return;
    uint16_t type=read16(f+12);
    if(type==0x0806&&n>=42){uint16_t op=read16(f+20);uint32_t sender,target;copy((uint8_t*)&sender,f+28,4);copy((uint8_t*)&target,f+38,4);neighbor_store(sender,f+22);if(op==1&&target==ip)arp_reply(f);return;}
    if(type!=0x0800||n<34)return;
    unsigned ihl=(unsigned)(f[14]&15u)*4;if(ihl<20||14u+ihl>n)return;
    uint16_t total=read16(f+16);if(total<ihl||(size_t)14+total>n)total=(uint16_t)(n-14);
    uint32_t source,target;copy((uint8_t*)&source,f+26,4);copy((uint8_t*)&target,f+30,4);if(target!=ip)return;
    const uint8_t*p=f+14+ihl;size_t length=total-ihl;
    if(f[23]==1&&length>=8){if(p[0]==8)icmp_reply(f,14+total);else if(p[0]==0)reply_sequence=read16(p+6);else if(p[0]==3){ping_error_type=p[0];ping_error_code=p[1];ping_error=1;}}
    else if(f[23]==17&&length>=8){uint16_t source_port=read16(p),destination=read16(p+2),udp_length=read16(p+4);if(source_port==53&&destination==53000&&udp_length>=8&&udp_length<=length)dns_receive(p+8,udp_length-8);else if(source_port==123&&destination==53001&&udp_length>=56&&udp_length<=length)ntp_receive(p+8,udp_length-8,source);}
    else if(f[23]==6)tcp_receive(p,length,source);
}

int network_init(void){ready=netdev_init();for(unsigned i=0;i<8;i++)neighbors[i].valid=0;ip_id=sequence=reply_sequence=0;dns_reply_id=0;dns_reply_status=0;if(ready)arp_request(gateway);return ready;}
int network_ready(void){return ready;}
void network_poll(void){if(!ready)return;uint8_t frame[1514];size_t n;for(unsigned i=0;i<16&&netdev_receive(frame,sizeof frame,&n);i++)process(frame,n);}
static neighbor_t*resolve_next_hop(uint32_t destination,unsigned spins){
    uint32_t hop=next_hop(destination);neighbor_t*n=neighbor_find(hop);if(n)return n;arp_request(hop);
    for(unsigned long i=0;i<spins;i++){if(process_cancel_requested())return 0;network_poll();n=neighbor_find(hop);if(n)return n;__asm__ volatile("pause");}return 0;
}

int network_ping(uint32_t address,unsigned spins,unsigned long*rounds,uint8_t*type,uint8_t*code){
    if(!ready)return -1;
    neighbor_t*n=resolve_next_hop(address,spins);if(!n)return -2;
    uint8_t f[98];size_t p=ipv4_begin(f,address,1,64,n->mac);f[p]=8;f[p+1]=0;write16(f+p+2,0);write16(f+p+4,0x6464);write16(f+p+6,++sequence);
    for(unsigned i=8;i<64;i++)f[p+i]=(uint8_t)i;
    write16(f+p+2,checksum(f+p,64));
    reply_sequence=0;ping_error=0;netdev_send(f,sizeof f);unsigned long i;
    for(i=0;i<spins&&reply_sequence!=sequence&&!ping_error;i++){network_poll();__asm__ volatile("pause");}
    if(rounds)*rounds=i;
    if(ping_error){if(type)*type=ping_error_type;if(code)*code=ping_error_code;return -3;}
    return reply_sequence==sequence?1:0;
}

int network_resolve(const char*name,uint32_t*address,unsigned spins){
    if(!ready||!name||!address)return -1;
    uint32_t numeric=network_parse_ipv4(name);if(numeric){*address=numeric;return 1;}
    neighbor_t*n=resolve_next_hop(dns_server,spins);if(!n)return -2;uint8_t f[512],q[256];size_t z=12,begin=0,name_length=0;
    while(name[name_length]){if(name_length>=253)return -3;name_length++;}
    while(begin<name_length){size_t end=begin;while(end<name_length&&name[end]!='.')end++;size_t length=end-begin;if(!length||length>63||z+length+1>=sizeof q)return -3;q[z++]=(uint8_t)length;for(size_t i=begin;i<end;i++)q[z++]=(uint8_t)name[i];begin=end+1;}
    q[z++]=0;write16(q+z,1);z+=2;write16(q+z,1);z+=2;dns_reply_id++;if(!dns_reply_id)dns_reply_id=1;
    write16(q,dns_reply_id);write16(q+2,0x0100);write16(q+4,1);write16(q+6,0);write16(q+8,0);write16(q+10,0);
    size_t p=ipv4_begin(f,dns_server,17,8+z,n->mac);write16(f+p,53000);write16(f+p+2,53);write16(f+p+4,(uint16_t)(8+z));write16(f+p+6,0);copy(f+p+8,q,z);
    dns_reply_status=0;dns_reply_address=0;netdev_send(f,p+8+z);for(unsigned long i=0;i<spins&&!dns_reply_status;i++){if(process_cancel_requested())return 0;network_poll();__asm__ volatile("pause");}
    if(dns_reply_status==1){*address=dns_reply_address;return 1;}return dns_reply_status?dns_reply_status:0;
}
int network_ntp_query(const char*server,uint64_t request_timestamp,network_ntp_result_t*result,unsigned spins){
    if(!ready||!server||!result||!request_timestamp)return NETWORK_NTP_DOWN;
    uint32_t address;if(network_resolve(server,&address,spins)!=1)return NETWORK_NTP_DNS;
    neighbor_t*n=resolve_next_hop(address,spins);if(!n)return NETWORK_NTP_ROUTE;
    uint8_t f[90],request[48];for(unsigned i=0;i<48;i++)request[i]=0;request[0]=0x23;
    write64(request+40,request_timestamp);
    size_t p=ipv4_begin(f,address,17,56,n->mac);
    write16(f+p,53001);write16(f+p+2,123);write16(f+p+4,56);write16(f+p+6,0);copy(f+p+8,request,48);
    ntp_server_ip=address;ntp_request_timestamp=request_timestamp;ntp_reply_status=0;
    if(!netdev_send(f,sizeof f))return NETWORK_NTP_SEND;
    for(unsigned long i=0;i<spins&&!ntp_reply_status;i++){network_poll();__asm__ volatile("pause");}
    if(ntp_reply_status==NETWORK_NTP_OK){*result=ntp_reply;return NETWORK_NTP_OK;}
    return ntp_reply_status?ntp_reply_status:NETWORK_NTP_TIMEOUT;
}

int network_tcp_connect(uint32_t address,uint16_t port,unsigned spins){
    if(!ready||!address||!port)return -1;
    if(!resolve_next_hop(address,spins))return -2;
    tcp_remote_ip=address;tcp_remote_port=port;tcp_local_port=(uint16_t)(49152+(ip_id&1023));
    tcp_local_seq=0x64000000u+ip_id;tcp_remote_seq=0;tcp_last_ack=tcp_local_seq;tcp_queue_length=0;tcp_state=1;
    for(unsigned i=0;i<8;i++)tcp_fragments[i].valid=0;
    if(!tcp_send(0x02,0,0))return -2;
    tcp_local_seq++;
    for(unsigned long i=0;i<spins&&tcp_state==1;i++){if(process_cancel_requested())return -6;network_poll();__asm__ volatile("pause");}if(tcp_state!=2)return tcp_state==-2?-5:-4;
    return 1;
}
int network_tcp_send(const void*data,size_t length,unsigned spins){
    if(tcp_state!=2||!data||!length)return -1;
    const uint8_t*p=(const uint8_t*)data;size_t sent=0;
    while(sent<length){
        size_t chunk=length-sent;if(chunk>1400)chunk=1400;uint8_t flags=(sent+chunk==length)?0x18:0x10;
        uint32_t start=tcp_local_seq,expected=start+(uint32_t)chunk;int acknowledged=0;
        for(unsigned attempt=0;attempt<3&&!acknowledged;attempt++){
            tcp_local_seq=start;if(!tcp_send(flags,p+sent,chunk))return -2;tcp_local_seq=expected;
            for(unsigned long wait=0;wait<spins&&!acknowledged&&tcp_state==2;wait++){if(process_cancel_requested())return -3;network_poll();if((int32_t)(tcp_last_ack-expected)>=0)acknowledged=1;__asm__ volatile("pause");}
        }
        if(!acknowledged)return -2;
        sent+=chunk;
    }
    return 1;
}
int network_tcp_receive(void*data,size_t capacity,size_t*length,unsigned spins){
    if(tcp_state<2||!data||!capacity||!length)return -1;
    for(unsigned long i=0;i<spins&&!tcp_queue_length&&tcp_state>=2;i++){if(process_cancel_requested()){*length=0;return -3;}network_poll();__asm__ volatile("pause");}
    size_t take=tcp_queue_length;if(take>capacity)take=capacity;for(size_t i=0;i<take;i++)((uint8_t*)data)[i]=tcp_queue[i];
    for(size_t i=take;i<tcp_queue_length;i++)tcp_queue[i-take]=tcp_queue[i];
    tcp_queue_length-=take;
    *length=take;return take?1:(tcp_state==4?0:-2);
}
void network_tcp_close(unsigned spins){
    if(tcp_state>=2&&tcp_state<4){tcp_send(0x11,0,0);tcp_local_seq++;for(unsigned long i=0;i<spins&&tcp_state<4;i++){network_poll();__asm__ volatile("pause");}}
    tcp_state=0;
}

int network_http_get(const char*host,const char*path,char*output,size_t capacity,size_t*length,unsigned spins){
    if(!host||!path||!output||!capacity||!length)return -1;
    uint32_t address;if(network_resolve(host,&address,spins)!=1)return -2;
    int connected=network_tcp_connect(address,80,spins);if(connected!=1)return connected==-2?-3:connected;
    uint8_t request[512];size_t q=0;const char*a="GET ",*b=" HTTP/1.0\r\nHost: ",*c="\r\nUser-Agent: OS64-curl/0.7\r\nConnection: close\r\nAccept: */*\r\n\r\n";
    while(*a)request[q++]=(uint8_t)*a++;
    while(*path&&q<sizeof request-1)request[q++]=(uint8_t)*path++;
    while(*b)request[q++]=(uint8_t)*b++;
    while(*host&&q<sizeof request-1)request[q++]=(uint8_t)*host++;
    while(*c&&q<sizeof request)request[q++]=(uint8_t)*c++;
    if(network_tcp_send(request,q,spins)!=1)return -3;
    size_t used=0;for(unsigned calls=0;calls<32&&used<capacity;calls++){size_t got=0;int r=network_tcp_receive(output+used,capacity-used,&got,spins);used+=got;if(r<=0)break;}
    *length=used;network_tcp_close(10000);return used?1:-4;
}

uint32_t network_parse_ipv4(const char*s){uint32_t v=0;for(unsigned part=0;part<4;part++){unsigned n=0,digits=0;while(*s>='0'&&*s<='9'){n=n*10u+(unsigned)(*s++-'0');digits++;}if(!digits||n>255)return 0;v|=n<<(part*8);if(part<3&&*s++!='.')return 0;}return *s?0:v;}
const char*network_driver(void){return netdev_name();}const uint8_t*network_mac(void){return netdev_mac();}
uint32_t network_address(void){return ip;}uint32_t network_gateway(void){return gateway;}uint32_t network_netmask(void){return netmask;}uint32_t network_dns_server(void){return dns_server;}
unsigned long network_rx_packets(void){return netdev_rx_packets();}unsigned long network_tx_packets(void){return netdev_tx_packets();}
