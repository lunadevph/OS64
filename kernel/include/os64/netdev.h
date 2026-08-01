#ifndef OS64_NETDEV_H
#define OS64_NETDEV_H
#include <stddef.h>
#include <stdint.h>
int netdev_init(void);
int netdev_ready(void);
const char *netdev_name(void);
const uint8_t *netdev_mac(void);
int netdev_send(const void *frame,size_t length);
int netdev_receive(void *frame,size_t capacity,size_t *length);
unsigned long netdev_rx_packets(void);
unsigned long netdev_tx_packets(void);
#endif
