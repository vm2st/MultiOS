/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
OUTPUT_FORMAT(binary)
ENTRY(_start)
SECTIONS { . = 0x7c00; .text : { *(.text) } /DISCARD/ : { *(.data*) *(.bss*) *(.note*) *(.eh_frame*) } }
