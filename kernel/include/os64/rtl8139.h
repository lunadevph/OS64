#ifndef OS64_RTL8139_H
#define OS64_RTL8139_H
#include <stddef.h>
#include <stdint.h>
int rtl8139_init(void);
int rtl8139_ready(void);
const uint8_t *rtl8139_mac(void);
int rtl8139_send(const void *frame,size_t length);
int rtl8139_receive(void *frame,size_t capacity,size_t *length);
unsigned long rtl8139_rx_packets(void);
unsigned long rtl8139_tx_packets(void);
#endif
