#ifndef OS64_SDK_NETWORK_H
#define OS64_SDK_NETWORK_H
int os_network_ready(void);int os_network_resolve(const char *hostname,unsigned *address);int os_network_tcp_connect(unsigned address,unsigned short port);int os_network_udp_send(unsigned address,unsigned short port,const void *data,unsigned size);
#endif
