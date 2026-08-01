#include "netdev.h"
#include "rtl8139.h"
#include "pcnet.h"
typedef enum{DEVICE_NONE,DEVICE_RTL8139,DEVICE_PCNET}device_t;
static device_t device;
int netdev_init(void){device=DEVICE_NONE;if(rtl8139_init()){device=DEVICE_RTL8139;return 1;}if(pcnet_init()){device=DEVICE_PCNET;return 1;}return 0;}
int netdev_ready(void){return device==DEVICE_RTL8139?rtl8139_ready():device==DEVICE_PCNET?pcnet_ready():0;}
const char*netdev_name(void){return device==DEVICE_RTL8139?"RTL8139":device==DEVICE_PCNET?"PCnet-FAST III":"none";}
const uint8_t*netdev_mac(void){return device==DEVICE_RTL8139?rtl8139_mac():device==DEVICE_PCNET?pcnet_mac():0;}
int netdev_send(const void*f,size_t n){return device==DEVICE_RTL8139?rtl8139_send(f,n):device==DEVICE_PCNET?pcnet_send(f,n):0;}
int netdev_receive(void*f,size_t c,size_t*n){return device==DEVICE_RTL8139?rtl8139_receive(f,c,n):device==DEVICE_PCNET?pcnet_receive(f,c,n):0;}
unsigned long netdev_rx_packets(void){return device==DEVICE_RTL8139?rtl8139_rx_packets():device==DEVICE_PCNET?pcnet_rx_packets():0;}
unsigned long netdev_tx_packets(void){return device==DEVICE_RTL8139?rtl8139_tx_packets():device==DEVICE_PCNET?pcnet_tx_packets():0;}
