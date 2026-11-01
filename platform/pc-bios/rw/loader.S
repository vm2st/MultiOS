/* vm2 studios | t.me/v2vproj — BIOS stage 2, i386+, E820 + A20 guards. */
.code16
.section .entry,"ax"
.global loader_entry
loader_entry:
 cli
 xorw %ax,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw $0x7c00,%sp
 cld
 movb %dl,bios_drive
 testb $0x80,%dl
 jz disk_error
 /* Require an E820 RAM interval covering all of [1MiB,30MiB).
  * Reject fragmented/reserved layouts rather than writing blindly. */
 xorl %ebx,%ebx
 movw $256,%bp
memory_loop:
 movw $0x7000,%di
 movl $1,20(%di)
 movl $0xe820,%eax
 movl $0x534d4150,%edx
 movl $24,%ecx
 sti
 int $0x15
 cli
 jc memory_error
 cmpl $0x534d4150,%eax
 jne memory_error
 cmpl $20,%ecx
 jb memory_error
 cmpl $24,%ecx
 jb memory_type
 testl $1,20(%di)
 jz memory_next
memory_type:
 cmpl $1,16(%di)
 jne memory_next
 cmpl $0,4(%di)
 jne memory_next
 cmpl $0x100000,0(%di)
 ja memory_next
 cmpl $0,12(%di)
 jne memory_ok
 movl 0(%di),%eax
 addl 8(%di),%eax
 jc memory_ok
 cmpl $0x1e00000,%eax
 jae memory_ok
memory_next:
 testl %ebx,%ebx
 jz memory_error
 decw %bp
 jnz memory_loop
memory_error:
 movw $memory_message,%si
 jmp print_error
disk_error:
 movw $disk_message,%si
 jmp print_error
memory_ok:
 sti
 movw $0x2401,%ax
 int $0x15
 cli
 /* Fast A20 where implemented, then VERIFY it before using high memory.
  * Unknown controllers fail closed; no claim of every BIOS platform. */
 inb $0x92,%al
 andb $0xfe,%al
 orb $2,%al
 outb %al,$0x92
 movw $0xffff,%ax
 movw %ax,%es
 movb 0x500,%al
 movb %al,%bl
 movb %es:0x510,%al
 movb %al,%bh
 movb $0,0x500
 movb $0xff,%es:0x510
 cmpb $0xff,0x500
 pushf
 movb %bh,%es:0x510
 movb %bl,0x500
 popf
 je a20_error
 xorw %ax,%ax
 movw %ax,%es
 /* Read all payload sectors through BIOS EDD while firmware is available.
  * Bounce buffer 0x10000; protected/unreal ES copy into verified high RAM.
  * No assumed ATA controller and no optional EDD flat-address extension. */
 movl $17,packet+8
 movw $0x7200,packet+4
 movw $0,packet+6
 call edd_read
 movl 0x7208,%eax
 cmpl $16,%eax
 jb payload_error
 cmpl $((2048-18)*512),%eax
 ja payload_error
 addl $511,%eax
 shrl $9,%eax
 movl %eax,sectors_left
 movl $18,packet+8
 movw $0,packet+4
 movw $0x1000,packet+6
 movl $0x100000,target
payload_loop:
 call edd_read
 lgdt gdtr
 movl %cr0,%eax
 orl $1,%eax
 movl %eax,%cr0
 ljmp $24,$unreal_pm
unreal_pm:
 movw $16,%ax
 movw %ax,%es
 movl %cr0,%eax
 andl $0xfffffffe,%eax
 movl %eax,%cr0
 ljmp $0,$unreal_rm
unreal_rm:
 cld
 movl $0x10000,%esi
 movl target,%edi
 movl $128,%ecx
 addr32 rep movsl
 xorw %ax,%ax
 movw %ax,%es
 addl $512,target
 addl $1,packet+8
 decl sectors_left
 jnz payload_loop
 lgdt gdtr
 movl %cr0,%eax
 orl $1,%eax
 movl %eax,%cr0
 ljmpl $8,$protected
edd_read:
 movw $packet,%si
 movb bios_drive,%dl
 movb $0x42,%ah
 sti
 int $0x13
 cli
 movw $0,%ax
 movw %ax,%ds
 movw %ax,%es
 jc payload_error
 ret
payload_error:
 movw $payload_message,%si
 jmp print_error
a20_error:
 movw $a20_message,%si
print_error:
 lodsb
 testb %al,%al
 jz halt
 movb $0x0e,%ah
 movw $7,%bx
 int $0x10
 jmp print_error
halt:
 cli
 hlt
 jmp halt
.align 8
gdt:
 .quad 0,0x00cf9a000000ffff,0x00cf92000000ffff,0x00009a000000ffff
gdtr:
 .word 31
 .long gdt
.align 8
packet:
 .byte 16,0
 .word 1
 .word 0,0
 .quad 17
bios_drive: .byte 0
.align 4
sectors_left: .long 0
target: .long 0x100000
payload_message: .asciz "MultiPulti: BIOS EDD payload I/O or size error"
memory_message: .asciz "MultiPulti vm2 studios: need E820 usable RAM 1..30MiB"
disk_message: .asciz "MultiPulti vm2 studios: BIOS EDD hard-disk boot required"
a20_message: .asciz "MultiPulti vm2 studios: A20 failed"
.code32
protected:
 movw $16,%ax
 movw %ax,%ds
 movw %ax,%es
 movw %ax,%ss
 movw %ax,%fs
 movw %ax,%gs
 movl $0x1e00000,%esp
 cld
 movl $__loader_bss_start,%edi
 movl $__loader_bss_end,%ecx
 subl %edi,%ecx
 xorl %eax,%eax
 rep stosb
 call loader_main
 jmp halt
.section .note.GNU-stack,"",@progbits
