#include "exceptions.h"
#include "panic.h"
#include <stdint.h>
typedef struct{uint16_t low,selector;uint8_t ist,attributes;uint16_t middle;uint32_t high,reserved;}__attribute__((packed))gate_t;
typedef struct{uint16_t limit;uint64_t base;}__attribute__((packed))idtr_t;
static gate_t idt[256];extern void isr_divide(void),isr_double(void),isr_opcode(void),isr_gp(void),isr_page(void);
static void gate(int n,void(*fn)(void)){uint64_t a=(uint64_t)(uintptr_t)fn;idt[n].low=a;idt[n].selector=8;idt[n].ist=0;idt[n].attributes=0x8e;idt[n].middle=a>>16;idt[n].high=a>>32;idt[n].reserved=0;}
void exceptions_init(void){gate(0,isr_divide);gate(6,isr_opcode);gate(8,isr_double);gate(13,isr_gp);gate(14,isr_page);idtr_t p={sizeof idt-1,(uint64_t)(uintptr_t)idt};__asm__ volatile("lidt %0"::"m"(p));}
__attribute__((noreturn))void exception_dispatch(uint64_t vector,uint64_t error){(void)error;static const char*reasons[]={"DIVIDE_ERROR","UNKNOWN","UNKNOWN","UNKNOWN","UNKNOWN","UNKNOWN","INVALID_OPCODE","UNKNOWN","DOUBLE_FAULT","UNKNOWN","UNKNOWN","UNKNOWN","UNKNOWN","GENERAL_PROTECTION_FAULT","PAGE_FAULT"};kernel_panic(vector<15?reasons[vector]:"CPU_EXCEPTION");}
