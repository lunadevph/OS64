ENTRY(_start)
SECTIONS {
 . = 0x800000;
 .text : { *(.text*) }
 .rodata : { *(.rodata*) }
 .init_array : {
  __init_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array))
  __init_array_end = .;
 }
 .fini_array : {
  __fini_array_start = .; KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array))
  __fini_array_end = .;
 }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
