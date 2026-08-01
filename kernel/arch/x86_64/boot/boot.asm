section .multiboot
align 8
header_start:
    dd 0xe85250d6
    dd 0
    dd header_end - header_start
    dd -(0xe85250d6 + 0 + (header_end - header_start))

    ; Ask a Multiboot2 loader for a linear 32-bit framebuffer.  The tag is
    ; optional so machines without a compatible video mode still boot into
    ; the VGA text fallback. Runtime mode changes are handled by display.c.
    dw 5
    dw 1
    dd 20
    dd 1024
    dd 768
    dd 32
    dd 0

    dw 0
    dw 0
    dd 8
header_end:

section .boot
bits 32
global start
extern kernel_main

start:
    cli
    mov esp, stack_top
    ; Preserve GRUB's Multiboot information pointer.  The page-table builder
    ; uses EDI as its table index, and kernel_main needs this value in RDI.
    mov esi, ebx

    ; CPUID must exist before we can query extended processor features.
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    test eax, 1 << 21
    jz .x64_required

    ; AMD and Intel expose long-mode support in extended CPUID.80000001h:EDX[29].
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .x64_required
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .x64_required

    mov eax, page_directory
    or eax, 3
    mov [pml4], eax
    mov eax, page_table0
    or eax, 3
    mov [page_directory], eax
    mov eax, page_table1
    or eax, 3
    mov [page_directory+8], eax
    mov eax, page_table2
    or eax, 3
    mov [page_directory+16], eax
    mov eax, page_table3
    or eax, 3
    mov [page_directory+24], eax

    xor edi, edi
.table:
    xor ecx, ecx
.map:
    mov eax, 0x200000
    mul ecx
    mov ebx, edi
    shl ebx, 30
    add eax, ebx
    or eax, 0x83
    ; Only the dedicated 8-16 MiB application window is user-accessible.
    test edi, edi
    jnz .supervisor_page
    cmp ecx, 4
    jb .supervisor_page
    cmp ecx, 8
    jae .supervisor_page
    or eax, 4
.supervisor_page:
    mov ebx, edi
    shl ebx, 12
    mov [page_table0 + ebx + ecx * 8], eax
    mov dword [page_table0 + ebx + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 512
    jne .map
    inc edi
    cmp edi, 4
    jne .table

    mov eax, pml4
    mov cr3, eax
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax
    lgdt [gdt.pointer]
    jmp 0x08:long_mode

.x64_required:
    mov edi, 0xb8000
    mov esi, x64_error
    mov ah, 0x4f
.error_write:
    lodsb
    test al, al
    jz .error_halt
    mov [edi], ax
    add edi, 2
    jmp .error_write
.error_halt:
    cli
    hlt
    jmp .error_halt

x64_error: db "OS64 boot error: x86-64 CPU with long mode is required.", 0

bits 64
long_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov rsp, stack_top
    xor rbp, rbp
    mov edi, esi
    call kernel_main
.hang:
    cli
    hlt
    jmp .hang

align 8
gdt:
    dq 0
    dq 0x00af9a000000ffff
    dq 0x00af92000000ffff
    ; Ring 3 code and data selectors (0x1b and 0x23).
    dq 0x00affa000000ffff
    dq 0x00aff2000000ffff
.pointer:
    dw $ - gdt - 1
    dq gdt

section .bss
align 4096
pml4: resb 4096
page_directory: resb 4096
page_table0: resb 4096
page_table1: resb 4096
page_table2: resb 4096
page_table3: resb 4096
stack_bottom: resb 32768
stack_top:

section .note.GNU-stack noalloc noexec nowrite progbits
