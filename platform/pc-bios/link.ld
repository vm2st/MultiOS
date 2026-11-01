/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
ENTRY(kstart)
SECTIONS {
 . = 0x10000;
 .text : { *(.start) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}

ASSERT(__bss_end < 0x70000, "MultiPulti low-memory image overlaps stack reserve")
