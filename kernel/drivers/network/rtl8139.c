#include "rtl8139.h"
#include "io.h"

#define PCI_CONFIG 0xcf8
#define PCI_DATA 0xcfc
#define RX_SIZE 8192u

static uint16_t io_base;
static uint8_t mac[6];
static uint8_t rx_buffer[RX_SIZE+16+1536] __attribute__((aligned(256)));
static uint8_t tx_buffer[4][1536] __attribute__((aligned(16)));
static uint32_t rx_offset,tx_index;
static unsigned long rx_count,tx_count;
static int present;

static uint32_t pci_read(unsigned bus,unsigned slot,unsigned function,unsigned offset){
    uint32_t address=0x80000000u|(bus<<16)|(slot<<11)|(function<<8)|(offset&0xfcu);
    outl(PCI_CONFIG,address);return inl(PCI_DATA);
}
static void pci_write(unsigned bus,unsigned slot,unsigned function,unsigned offset,uint32_t value){
    uint32_t address=0x80000000u|(bus<<16)|(slot<<11)|(function<<8)|(offset&0xfcu);
    outl(PCI_CONFIG,address);outl(PCI_DATA,value);
}

int rtl8139_init(void){
    present=0;rx_offset=tx_index=0;rx_count=tx_count=0;
    unsigned found_bus=0,found_slot=0,found_fn=0;
    for(unsigned bus=0;bus<256&&!present;bus++)for(unsigned slot=0;slot<32&&!present;slot++)for(unsigned fn=0;fn<8;fn++){
        uint32_t id=pci_read(bus,slot,fn,0);
        if((id&0xffffu)==0x10ecu&&(id>>16)==0x8139u){found_bus=bus;found_slot=slot;found_fn=fn;present=1;break;}
        if(fn==0&&id==0xffffffffu)break;
    }
    if(!present)return 0;
    uint32_t bar=pci_read(found_bus,found_slot,found_fn,0x10);
    if(!(bar&1u)){present=0;return 0;}
    io_base=(uint16_t)(bar&~3u);
    uint32_t command=pci_read(found_bus,found_slot,found_fn,0x04);
    pci_write(found_bus,found_slot,found_fn,0x04,command|5u);
    outb(io_base+0x52,0);
    outb(io_base+0x37,0x10);
    for(unsigned n=0;n<100000&&inb(io_base+0x37)&0x10;n++)__asm__ volatile("pause");
    if(inb(io_base+0x37)&0x10){present=0;return 0;}
    for(unsigned i=0;i<6;i++)mac[i]=inb(io_base+i);
    outl(io_base+0x30,(uint32_t)(uintptr_t)rx_buffer);
    outw(io_base+0x3c,0);
    outw(io_base+0x3e,0xffff);
    outl(io_base+0x44,0x0000000fu);
    outb(io_base+0x37,0x0c);
    return 1;
}

int rtl8139_ready(void){return present;}
const uint8_t*rtl8139_mac(void){return mac;}

int rtl8139_send(const void*frame,size_t length){
    if(!present||!frame||length<14||length>1514)return 0;
    const uint8_t*s=frame;uint8_t*d=tx_buffer[tx_index];
    for(size_t i=0;i<length;i++)d[i]=s[i];
    if(length<60){for(size_t i=length;i<60;i++)d[i]=0;length=60;}
    outl(io_base+0x20+tx_index*4,(uint32_t)(uintptr_t)d);
    outl(io_base+0x10+tx_index*4,(uint32_t)length);
    tx_index=(tx_index+1)&3u;tx_count++;return 1;
}

int rtl8139_receive(void*frame,size_t capacity,size_t*length){
    if(!present||!frame||!length||(inb(io_base+0x37)&1u))return 0;
    uint8_t*h=rx_buffer+(rx_offset%RX_SIZE);
    uint16_t status=(uint16_t)(h[0]|((uint16_t)h[1]<<8));
    uint16_t size=(uint16_t)(h[2]|((uint16_t)h[3]<<8));
    if(!(status&1u)||size<4||size>1518){rx_offset=0;outw(io_base+0x38,0xfff0);return 0;}
    size=(uint16_t)(size-4);
    if(size>capacity)size=(uint16_t)capacity;
    uint8_t*d=frame;
    for(uint16_t i=0;i<size;i++)d[i]=rx_buffer[(rx_offset+4+i)%RX_SIZE];
    rx_offset=(rx_offset+((uint32_t)h[2]|((uint32_t)h[3]<<8))+4+3)&~3u;
    rx_offset%=RX_SIZE;outw(io_base+0x38,(uint16_t)(rx_offset-16));
    *length=size;rx_count++;return 1;
}
unsigned long rtl8139_rx_packets(void){return rx_count;}
unsigned long rtl8139_tx_packets(void){return tx_count;}
