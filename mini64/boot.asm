section .multiboot
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + 0 + (header_end - header_start))
    dw 5
    dw 1
    dd 20
    dd 1024
    dd 768
    dd 32
    align 8
    dw 0
    dw 0
    dd 8
header_end:

section .text
global start
extern kernel_main

start:
    cli
    mov esp, stack_top
    push ebx
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
stack_bottom: resb 16384
stack_top:
