#ifndef OS64_PCNET_H
#define OS64_PCNET_H
#include <stddef.h>
#include <stdint.h>
int pcnet_init(void);
int pcnet_ready(void);
const uint8_t *pcnet_mac(void);
int pcnet_send(const void *frame,size_t length);
int pcnet_receive(void *frame,size_t capacity,size_t *length);
unsigned long pcnet_rx_packets(void);
unsigned long pcnet_tx_packets(void);
#endif
