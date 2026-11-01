/* vm2 studios | t.me/v2vproj — i386, no SIMD/FPU/CPUID */
.code32
.section .text
.extern multi_pc_fault_panic
.extern multi_pc_double_fault
.altmacro
.macro FAULT_STUB num
.global multi_fault_stub_\num
multi_fault_stub_\num:
 /* Error-code exceptions (including later extensions, if enabled by a future port).
  * #DF uses a task gate, not this ordinary interrupt stub. */
 .if (\num != 8) && (\num != 10) && (\num != 11) && (\num != 12) && (\num != 13) && (\num != 14) && (\num != 17) && (\num != 21) && (\num != 29) && (\num != 30)
 pushl $0
 .endif
 pushl $\num
 jmp multi_fault_common
.endm
.set vec,0
.rept 256
 FAULT_STUB %vec
 .set vec,vec+1
.endr
multi_fault_common:
 cli
 cld
 pushal
 xorl %eax,%eax
 movw %ds,%ax
 pushl %eax
 movw %es,%ax
 pushl %eax
 movw %fs,%ax
 pushl %eax
 movw %gs,%ax
 pushl %eax
 movw $0x10,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%fs
 movw %ax,%gs
 movl %esp,%eax
 andl $-16,%esp
 subl $12,%esp
 pushl %eax
 call multi_pc_fault_panic
1: cli
 hlt
 jmp 1b
.global multi_fault_df_entry
multi_fault_df_entry:
 cli
 cld
 /* Task switch pushed the #DF error code. C does not inspect that stack. */
 andl $-16,%esp
 call multi_pc_double_fault
2: cli
 hlt
 jmp 2b
.section .rodata
.align 4
.global multi_fault_stubs
multi_fault_stubs:
.macro STUB_ADDRESS num
 .long multi_fault_stub_\num
.endm
.set vec,0
.rept 256
 STUB_ADDRESS %vec
 .set vec,vec+1
.endr
.section .note.GNU-stack,"",@progbits
