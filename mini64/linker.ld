OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

SECTIONS
{
    . = 1M;

    .multiboot : { *(.multiboot) }

    .text : { *(.text*) }

    .rodata : { *(.rodata*) }

    .data : { *(.data*) }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}
