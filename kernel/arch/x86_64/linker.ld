ENTRY(start)

SECTIONS
{
    . = 1M;
    .boot ALIGN(8) : { KEEP(*(.multiboot)) *(.boot) }
    .text ALIGN(4K) : { *(.text*) }
    .rodata ALIGN(4K) : { *(.rodata*) }
    .data ALIGN(4K) : { *(.data*) }
    .bss ALIGN(4K) : { *(COMMON) *(.bss*) }
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
