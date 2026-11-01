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
 movw %ax,%es
 movw %ax,%ss
 movw $0x7c00,%sp
 cld
 /* 8086/186: high FLAGS bits stuck at 1; 286 real mode: stuck at 0.
  * Only 16-bit instructions here, before any 386-only operation. */
 pushf
 popw %cx
 movw %cx,%ax
 andw $0x0fff,%ax
 pushw %ax
 popf
 pushf
 popw %ax
 andw $0xf000,%ax
 cmpw $0xf000,%ax
 je old_cpu
 movw %cx,%ax
 orw $0xf000,%ax
 pushw %ax
 popf
 pushf
 popw %ax
 andw $0xf000,%ax
 jz old_cpu
 pushw %cx
 popf
 sti
 movb %dl,drive
 movw $0x55aa,%bx
 movb $0x41,%ah
 int $0x13
 jc fail
 cmpw $0xaa55,%bx
 jne fail
 testw $1,%cx
 jz fail
 movw $16,%bp
load:
 movw $packet,%si
 movb drive,%dl
 movb $0x42,%ah
 int $0x13
 jc fail
 addw $0x20,packet+6
 addl $1,packet+8
 decw %bp
 jnz load
 movb drive,%dl
 ljmp $0,$0x8000
old_cpu:
 pushw %cx
 popf
 movw $cpu_error,%si
 jmp print
fail:
 movw $error,%si
print:
 lodsb
 testb %al,%al
 jz halt
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp print
halt:
 cli
 hlt
 jmp halt
.align 8
gdt:
 .quad 0
 .quad 0x00cf9a000000ffff
 .quad 0x00cf92000000ffff
gdtr:
 .word 23
 .long gdt
packet:
 .byte 16,0
 .word 1
 .word 0,0x0800
 .quad 1
drive: .byte 0
cpu_error: .asciz "MultiPulti vm2 studios: i386+ required"
error: .asciz "MultiPulti vm2 studios: EDD error"
.code32
protected:
 movw $16,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw %ax,%fs
 movw %ax,%gs
 movl $0x90000,%esp
 cld
 movl $0x10000,%eax
 jmp *%eax
.org 432
.ascii "MP22"
.word 16,1
.org 440
/* Standard MBR disk signature is filled by image generator. */
.long 0
.word 0
.space 64
.word 0xaa55
.section .note.GNU-stack,"",@progbits
