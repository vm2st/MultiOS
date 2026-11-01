/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
.code32
.section .start,"ax"
.global kstart
.extern multipulti_main
.extern multi_pc_fault_init
.extern __bss_start
.extern __bss_end
kstart:
 cld
 movl $__bss_start,%edi
 movl $__bss_end,%ecx
 subl %edi,%ecx
 xorl %eax,%eax
 rep stosb
 call multi_pc_fault_init
 call multipulti_main
1: cli
 hlt
 jmp 1b
.section .note.GNU-stack,"",@progbits
