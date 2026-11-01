ENTRY(loader_entry)
SECTIONS {
 . = 0x8000;
 .text : { *(.entry) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __loader_bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __loader_bss_end = .;
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
ASSERT(__loader_bss_end < 0x9e00, "Stage 2 exceeds reserved low RAM")
