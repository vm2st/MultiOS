/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
.code16
.section .text
.global _start
_start:
 ljmp $0,$start
start:
 cli
 xorw %ax,%ax
 movw %ax,%ds
 movw %ax,%ss
 movw $0x7c00,%sp
 sti
 movw $message,%si
1: lodsb
 testb %al,%al
 jz 2f
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp 1b
2: cli
 hlt
 jmp 2b
message: .asciz "MultiOS Live USB: select x64 UEFI boot. Legacy USB mode is not supported."
.org 446
.space 64
.word 0xaa55
.section .note.GNU-stack,"",@progbits
