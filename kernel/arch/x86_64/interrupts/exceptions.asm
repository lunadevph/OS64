bits 64
extern exception_dispatch
global isr_divide,isr_opcode,isr_double,isr_gp,isr_page
isr_divide: push qword 0
            push qword 0
            jmp isr_common
isr_opcode: push qword 0
            push qword 6
            jmp isr_common
isr_double: push qword 8
            jmp isr_common
isr_gp:     push qword 13
            jmp isr_common
isr_page:   push qword 14
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi,[rsp+120]
    mov rsi,[rsp+128]
    call exception_dispatch
    cli
.halt: hlt
    jmp .halt
section .note.GNU-stack noalloc noexec nowrite progbits
