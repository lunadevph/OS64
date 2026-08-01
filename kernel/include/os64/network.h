#ifndef OS64_NETWORK_H
#define OS64_NETWORK_H
#include <stdint.h>
#include <stddef.h>
int network_init(void);
int network_ready(void);
const char *network_driver(void);
void network_poll(void);
int network_ping(uint32_t address,unsigned spins,unsigned long *rounds,uint8_t *icmp_type,uint8_t *icmp_code);
int network_resolve(const char *name,uint32_t *address,unsigned spins);
enum {
 NETWORK_NTP_OK=1,NETWORK_NTP_DOWN=-1,NETWORK_NTP_DNS=-2,
 NETWORK_NTP_ROUTE=-3,NETWORK_NTP_SEND=-4,NETWORK_NTP_TIMEOUT=-5,
 NETWORK_NTP_SHORT=-6,NETWORK_NTP_MODE=-7,NETWORK_NTP_LEAP=-8,
 NETWORK_NTP_STRATUM=-9,NETWORK_NTP_ORIGIN=-10,
 NETWORK_NTP_TIMESTAMP=-11
};
typedef struct {
 uint32_t address;
 uint8_t stratum;
 uint64_t receive_timestamp;
 uint64_t transmit_timestamp;
} network_ntp_result_t;
int network_ntp_query(const char *server,uint64_t request_timestamp,
 network_ntp_result_t *result,unsigned spins);
int network_http_get(const char *host,const char *path,char *output,size_t capacity,size_t *length,unsigned spins);
int network_tcp_connect(uint32_t address,uint16_t port,unsigned spins);
int network_tcp_send(const void *data,size_t length,unsigned spins);
int network_tcp_receive(void *data,size_t capacity,size_t *length,unsigned spins);
void network_tcp_close(unsigned spins);
uint32_t network_parse_ipv4(const char *text);
const uint8_t *network_mac(void);
uint32_t network_address(void);
uint32_t network_gateway(void);
uint32_t network_netmask(void);
uint32_t network_dns_server(void);
unsigned long network_rx_packets(void);
unsigned long network_tx_packets(void);
#endif
