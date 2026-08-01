#include "elf.h"
#include <stdint.h>
typedef struct{unsigned char ident[16];uint16_t type,machine;uint32_t version;uint64_t entry,phoff,shoff;uint32_t flags;uint16_t ehsize,phentsize,phnum,shentsize,shnum,shstrndx;}__attribute__((packed))ehdr_t;
typedef struct{uint32_t type,flags;uint64_t offset,vaddr,paddr,filesz,memsz,align;}__attribute__((packed))phdr_t;
int elf_execute(const unsigned char*img,size_t size,const char*args,const os64_api_t*api){
    if(size<sizeof(ehdr_t))return -1;
    const ehdr_t*h=(const ehdr_t*)img;
    if(h->ident[0]!=0x7f||h->ident[1]!='E'||h->ident[2]!='L'||h->ident[3]!='F'||h->ident[4]!=2||h->machine!=62||h->type!=2)return -2;
    if(h->phoff+(uint64_t)h->phnum*h->phentsize>size||h->phentsize<sizeof(phdr_t))return -3;
    for(uint16_t i=0;i<h->phnum;i++){const phdr_t*p=(const phdr_t*)(img+h->phoff+(uint64_t)i*h->phentsize);if(p->type!=1)continue;if(p->offset+p->filesz>size||p->filesz>p->memsz||p->vaddr<0x800000||p->vaddr+p->memsz>0x1000000)return -4;unsigned char*d=(unsigned char*)(uintptr_t)p->vaddr;for(uint64_t n=0;n<p->filesz;n++)d[n]=img[p->offset+n];for(uint64_t n=p->filesz;n<p->memsz;n++)d[n]=0;}
    if(h->entry<0x800000||h->entry>=0x1000000)return -5;
    return ((os64_entry_t)(uintptr_t)h->entry)(api,args);
}
