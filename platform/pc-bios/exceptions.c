/* vm2 studios | t.me/v2vproj — fail-stop diagnostics, not fault recovery. */
#include "exceptions.h"
#include "io.h"
#include <stddef.h>

typedef struct __attribute__((packed)) { u16 lo,selector;u8 zero,type;u16 hi; } Gate;
typedef struct __attribute__((packed)) {u16 limit;u32 base;} TablePtr;
typedef struct __attribute__((packed)) {
    u32 link,esp0,ss0,esp1,ss1,esp2,ss2,cr3,eip,eflags;
    u32 eax,ecx,edx,ebx,esp,ebp,esi,edi;
    u32 es,cs,ss,ds,fs,gs,ldt;
    u16 trap,iomap;
} Tss;
_Static_assert(sizeof(Gate)==8,"IDT gate size");
_Static_assert(sizeof(Tss)==104,"i386 TSS size");
_Static_assert(offsetof(Tss,eip)==32 && offsetof(Tss,esp)==56,"TSS offsets");
_Static_assert(sizeof(MultiFaultFrame)==68,"fault frame size");
_Static_assert(offsetof(MultiFaultFrame,vector)==48,"assembly frame vector");
_Static_assert(offsetof(MultiFaultFrame,eip)==56,"assembly frame EIP");
extern const u32 multi_fault_stubs[256];
extern void multi_fault_df_entry(void);
static Gate idt[256] __attribute__((aligned(16)));
static u32 gdt[10] __attribute__((aligned(8)));
static Tss normal_tss,df_tss;
static u8 df_stack[8192] __attribute__((aligned(16)));
/* Fixed diagnostic canary, NOT an ASLR/security secret. Never changed at runtime. */
u32 __stack_chk_guard=0x4d50554c;
static volatile u32 panicking;
static u32 vga_pos;

static void byte_out(char c) {
    /* No HAL, allocation, filesystem, lock or input dependency. Bounded UART wait. */
    if(c=='\n')vga_pos=(vga_pos/80+1)*80;
    else if(vga_pos<2000)((volatile u16 *)0xb8000)[vga_pos++]=0x4f00|(u8)c;
    for(u32 i=0;i<10000;i++)if(ib(0x3fd)&32){ob(0x3f8,(u8)c);break;}
}
static void text(const char *s){while(*s)byte_out(*s++);}
static void hex(u32 x){static const char h[]="0123456789ABCDEF";text("0x");for(int n=28;n>=0;n-=4)byte_out(h[(x>>n)&15]);}
static void field(const char *label,u32 x){text(label);hex(x);byte_out(' ');}
static void header(void){
    __asm__ volatile("cli;cld":::"memory");
    /* A fault before module setup must still have a usable COM1 diagnostic. */
    ob(0x3f9,0);ob(0x3fb,0x80);ob(0x3f8,1);ob(0x3f9,0);
    ob(0x3fb,3);ob(0x3fa,0xc7);ob(0x3fc,0x0b);
    for(u32 i=0;i<2000;i++)((volatile u16 *)0xb8000)[i]=0x4f20;
    vga_pos=0;
    text("\nMULTIPULTI KERNEL PANIC - MultiOS\nvm2 studios | t.me/v2vproj\n");
    text("UNPUBLISHED DEVELOPMENT - CPU stopped; no filesystem recovery attempted.\n");
}
static void halt(void) __attribute__((noreturn));
static void halt(void){text("\nHALTED - preserve the diagnostic; restart manually.\n");for(;;)__asm__ volatile("cli;hlt");}
static const char *exception_name(u32 n){
    static const char *const names[32]={"Divide error","Debug","NMI","Breakpoint","Overflow","BOUND range","Invalid opcode","Device unavailable","Double fault","Reserved 9","Invalid TSS","Segment not present","Stack fault","General protection","Page fault","Reserved 15","x87 error","Alignment check","Machine check","SIMD error","Virtualization","Control protection","Reserved 22","Reserved 23","Reserved 24","Reserved 25","Reserved 26","Reserved 27","Hypervisor injection","VMM communication","Security","Reserved 31"};
    return n<32?names[n]:"Unexpected interrupt";
}
void multi_pc_fault_panic(const MultiFaultFrame *f){
    __asm__ volatile("cli":::"memory");
    /* The emergency #DF path does not depend on this flag or the ordinary stack. */
    if(panicking)for(;;)__asm__ volatile("hlt");
    panicking=1;header();text(exception_name(f->vector));byte_out('\n');
    field("VECTOR=",f->vector);field("ERROR=",f->error);byte_out('\n');
    field("EIP=",f->eip);field("CS=",f->cs);field("EFLAGS=",f->eflags);byte_out('\n');
    field("EAX=",f->eax);field("EBX=",f->ebx);field("ECX=",f->ecx);field("EDX=",f->edx);byte_out('\n');
    field("ESI=",f->esi);field("EDI=",f->edi);field("EBP=",f->ebp);byte_out('\n');
    /* pushad_esp points to vector/error, 20 bytes below the interrupted ESP. */
    field("ESP=",f->pushad_esp+20);field("DS=",f->ds);field("ES=",f->es);byte_out('\n');
    field("FS=",f->fs);field("GS=",f->gs);byte_out('\n');
    if(f->vector==14){u32 cr2;__asm__ volatile("movl %%cr2,%0":"=r"(cr2));field("CR2=",cr2);byte_out('\n');}
    halt();
}
void multi_pc_double_fault(void){
    header();text("Double fault - emergency TSS / independent 8 KiB stack\n");
    field("VECTOR=",8);field("ERROR=",0);byte_out('\n');
    text("Interrupted task state (fault during exception delivery):\n");
    field("EIP=",normal_tss.eip);field("CS=",normal_tss.cs);field("EFLAGS=",normal_tss.eflags);byte_out('\n');
    field("ESP=",normal_tss.esp);field("SS=",normal_tss.ss);byte_out('\n');halt();
}
void __stack_chk_fail(void) __attribute__((noreturn));
void __stack_chk_fail(void){
    __asm__ volatile("cli":::"memory");
    if(panicking)for(;;)__asm__ volatile("hlt");
    panicking=1;header();text("STACK CANARY FAILURE - compiler-detected stack corruption\n");
    text("Location unavailable; do not continue or write to the filesystem.\n");halt();
}
static void tss_descriptor(int slot,Tss *t){
    u32 base=(u32)t,limit=sizeof(Tss)-1;
    gdt[slot*2]=(limit&0xffff)|((base&0xffff)<<16);
    gdt[slot*2+1]=((base>>16)&255)|0x00008900|(base&0xff000000);
}
void multi_pc_fault_init(void){
    __asm__ volatile("cli":::"memory");
    /* Flat code/data selectors preserve the boot GDT ABI (08/10). */
    gdt[2]=0x0000ffff;gdt[3]=0x00cf9a00;
    gdt[4]=0x0000ffff;gdt[5]=0x00cf9200;
    normal_tss.ss0=0x10;normal_tss.esp0=0x90000;normal_tss.iomap=sizeof(Tss);
    df_tss.eip=(u32)multi_fault_df_entry;df_tss.eflags=2;
    df_tss.esp=(u32)(df_stack+sizeof(df_stack));df_tss.ebp=df_tss.esp;
    df_tss.cs=8;df_tss.ss=df_tss.ds=df_tss.es=df_tss.fs=df_tss.gs=0x10;
    df_tss.ss0=0x10;df_tss.esp0=df_tss.esp;df_tss.iomap=sizeof(Tss);
    __asm__ volatile("movl %%cr3,%0":"=r"(df_tss.cr3));
    tss_descriptor(3,&normal_tss);tss_descriptor(4,&df_tss);
    TablePtr gdtr={sizeof(gdt)-1,(u32)gdt};
    __asm__ volatile("lgdt %0"::"m"(gdtr):"memory");
    u16 task=0x18;__asm__ volatile("ltr %0"::"r"(task):"memory");
    for(u32 i=0;i<256;i++){
        u32 a=multi_fault_stubs[i];idt[i]=(Gate){a&0xffff,8,0,0x8e,a>>16};
    }
    idt[8]=(Gate){0,0x20,0,0x85,0}; /* present i386 task gate */
    /* IF remains clear throughout this polling kernel. Mask legacy IRQs too.
     * PIC remapping/IRQ handlers are deliberately NOT implemented here. */
    ob(0x21,0xff);ob(0xa1,0xff);
    TablePtr idtr={sizeof(idt)-1,(u32)idt};
    __asm__ volatile("lidt %0"::"m"(idtr):"memory");
#ifdef MULTIOS_FAULT_TEST
#if MULTIOS_FAULT_TEST == 1
    __asm__ volatile(".byte 0x0f,0x0b");
#elif MULTIOS_FAULT_TEST == 2
    __asm__ volatile("xorl %%edx,%%edx; xorl %%ecx,%%ecx; movl $1,%%eax; divl %%ecx":::"eax","ecx","edx","cc");
#elif MULTIOS_FAULT_TEST == 3
    __asm__ volatile("movw $0xfff8,%%ax; movw %%ax,%%ds":::"eax");
#elif MULTIOS_FAULT_TEST == 4
    /* A contributory exception while delivering #GP must invoke the #DF task. */
    idt[13].type=0;
    __asm__ volatile("movw $0xfff8,%%ax; movw %%ax,%%ds":::"eax","memory");
#elif MULTIOS_FAULT_TEST == 5
    __asm__ volatile("int $0x81");
#elif MULTIOS_FAULT_TEST == 6
    extern int multi_stack_fault_test(void);
    (void)multi_stack_fault_test();
#else
#error Unsupported fault test number
#endif
#endif
}
