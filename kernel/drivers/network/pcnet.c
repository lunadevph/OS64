#include "pcnet.h"
#include "io.h"

#define PCI_CONFIG 0xcf8
#define PCI_DATA 0xcfc
#define RING_COUNT 8u
#define BUFFER_SIZE 1536u

typedef struct{uint32_t address,flags,flags2,available;}__attribute__((packed,aligned(16))) descriptor_t;
typedef struct{
    uint16_t mode,ring_lengths;
    uint8_t mac[6];
    uint16_t reserved;
    uint8_t filter[8];
    uint32_t rx_ring,tx_ring;
}__attribute__((packed,aligned(4))) init_block_t;

static uint16_t io_base;
static uint8_t mac[6];
static descriptor_t rx_ring[RING_COUNT]__attribute__((aligned(16)));
static descriptor_t tx_ring[RING_COUNT]__attribute__((aligned(16)));
static uint8_t rx_buffer[RING_COUNT][BUFFER_SIZE]__attribute__((aligned(16)));
static uint8_t tx_buffer[RING_COUNT][BUFFER_SIZE]__attribute__((aligned(16)));
static init_block_t init_block __attribute__((aligned(16)));
static unsigned rx_index,tx_index;
static unsigned long rx_count,tx_count;
static int present;

static uint32_t pci_read(unsigned bus,unsigned slot,unsigned fn,unsigned offset){outl(PCI_CONFIG,0x80000000u|(bus<<16)|(slot<<11)|(fn<<8)|(offset&0xfcu));return inl(PCI_DATA);}
static void pci_write(unsigned bus,unsigned slot,unsigned fn,unsigned offset,uint32_t value){outl(PCI_CONFIG,0x80000000u|(bus<<16)|(slot<<11)|(fn<<8)|(offset&0xfcu));outl(PCI_DATA,value);}
static uint16_t csr_read(uint16_t reg){outw(io_base+0x12,reg);return inw(io_base+0x10);}
static void csr_write(uint16_t reg,uint16_t value){outw(io_base+0x12,reg);outw(io_base+0x10,value);}
static void bcr_write(uint16_t reg,uint16_t value){outw(io_base+0x12,reg);outw(io_base+0x16,value);}

int pcnet_init(void){
    present=0;unsigned fb=0,fs=0,ff=0;
    for(unsigned bus=0;bus<256&&!present;bus++)for(unsigned slot=0;slot<32&&!present;slot++)for(unsigned fn=0;fn<8;fn++){
        uint32_t id=pci_read(bus,slot,fn,0);
        if((id&0xffffu)==0x1022u&&(id>>16)==0x2000u){fb=bus;fs=slot;ff=fn;present=1;break;}
        if(fn==0&&id==0xffffffffu)break;
    }
    if(!present)return 0;
    uint32_t bar=pci_read(fb,fs,ff,0x10);if(!(bar&1u)){present=0;return 0;}io_base=(uint16_t)(bar&~3u);
    pci_write(fb,fs,ff,0x04,pci_read(fb,fs,ff,0x04)|5u);
    (void)inw(io_base+0x14);for(unsigned n=0;n<10000;n++)__asm__ volatile("pause");
    bcr_write(20,2);csr_write(0,4);
    for(unsigned i=0;i<6;i++)mac[i]=inb(io_base+i);
    for(unsigned i=0;i<RING_COUNT;i++){
        rx_ring[i].address=(uint32_t)(uintptr_t)rx_buffer[i];rx_ring[i].flags=0x8000fa00u;rx_ring[i].flags2=rx_ring[i].available=0;
        tx_ring[i].address=(uint32_t)(uintptr_t)tx_buffer[i];tx_ring[i].flags=tx_ring[i].flags2=tx_ring[i].available=0;
    }
    init_block.mode=0;init_block.ring_lengths=(uint16_t)((3u<<4)|(3u<<12));
    for(unsigned i=0;i<6;i++)init_block.mac[i]=mac[i];
    init_block.reserved=0;
    for(unsigned i=0;i<8;i++)init_block.filter[i]=0;
    init_block.rx_ring=(uint32_t)(uintptr_t)rx_ring;init_block.tx_ring=(uint32_t)(uintptr_t)tx_ring;
    uint32_t address=(uint32_t)(uintptr_t)&init_block;csr_write(1,(uint16_t)address);csr_write(2,(uint16_t)(address>>16));
    csr_write(0,1);unsigned n;for(n=0;n<1000000&&!(csr_read(0)&0x0100);n++)__asm__ volatile("pause");
    if(n==1000000){present=0;return 0;}csr_write(0,0x0100);csr_write(0,2);
    rx_index=tx_index=0;rx_count=tx_count=0;return 1;
}

int pcnet_ready(void){return present;}const uint8_t*pcnet_mac(void){return mac;}
int pcnet_send(const void*frame,size_t length){
    if(!present||!frame||length<14||length>1514)return 0;
    descriptor_t*d=&tx_ring[tx_index];
    if(d->flags&0x80000000u)return 0;
    const uint8_t*s=frame;for(size_t i=0;i<length;i++)tx_buffer[tx_index][i]=s[i];
    if(length<60){for(size_t i=length;i<60;i++)tx_buffer[tx_index][i]=0;length=60;}
    d->flags2=0;d->flags=0x8300f000u|((uint32_t)(-(int32_t)length)&0x0fffu);
    csr_write(0,8);tx_index=(tx_index+1)&(RING_COUNT-1);tx_count++;return 1;
}
int pcnet_receive(void*frame,size_t capacity,size_t*length){
    if(!present||!frame||!length)return 0;
    descriptor_t*d=&rx_ring[rx_index];if(d->flags&0x80000000u)return 0;
    size_t n=d->flags2&0x0fffu;if(n>=4)n-=4;if(n>capacity)n=capacity;uint8_t*out=frame;
    int good=(d->flags&0x40000000u)==0&&(d->flags&0x03000000u)==0x03000000u;
    if(good)for(size_t i=0;i<n;i++)out[i]=rx_buffer[rx_index][i];
    d->flags2=0;d->flags=0x8000fa00u;rx_index=(rx_index+1)&(RING_COUNT-1);
    if(!good)return 0;
    *length=n;rx_count++;return 1;
}
unsigned long pcnet_rx_packets(void){return rx_count;}unsigned long pcnet_tx_packets(void){return tx_count;}
