ENTRY(_start)
SECTIONS { . = 0x800000; .text : { *(.text*) } .rodata : { *(.rodata*) } .data : { *(.data*) } .bss : { *(.bss*) *(COMMON) } /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) } }
