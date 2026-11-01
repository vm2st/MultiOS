/* vm2 studios | t.me/v2vproj — extended i386 NTFS kernel. */
ENTRY(kstart)
SECTIONS {
 . = 0x100000;
 .text : { *(.start) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 __bss_start = .;
 .bss : { *(.bss*) *(COMMON) }
 __bss_end = .;
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
ASSERT(__bss_end < 0x400000, "Kernel overlaps NTFS arena")
