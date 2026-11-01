/* vm2 studios | t.me/v2vproj — guarded CPUID; no FPU/SIMD needed. */
#include "multios/cpu.h"
#include "cpu.h"
#ifndef MULTIOS_X86_NO_CPUID
#define MULTIOS_X86_NO_CPUID 0
#endif
#if !MULTIOS_X86_NO_CPUID
static int has_cpuid(void) {
#if defined(__x86_64__)
    uint64_t before,after;
    __asm__ volatile("pushfq; popq %0" : "=r"(before));
    uint64_t changed=before^((uint64_t)1<<21);
    __asm__ volatile("pushq %0; popfq" ::"r"(changed):"cc","memory");
    __asm__ volatile("pushfq; popq %0" : "=r"(after));
    __asm__ volatile("pushq %0; popfq" ::"r"(before):"cc","memory");
#else
    u32 before,after;
    __asm__ volatile("pushfl; popl %0" : "=r"(before));
    u32 changed=before^(1u<<21);
    __asm__ volatile("pushl %0; popfl" ::"r"(changed):"cc","memory");
    __asm__ volatile("pushfl; popl %0" : "=r"(after));
    __asm__ volatile("pushl %0; popfl" ::"r"(before):"cc","memory");
#endif
    return ((before^after)&(1u<<21))!=0;
}
static void leaf(u32 n,u32 *a,u32 *b,u32 *c,u32 *d) {
    /* Explicit optional opcode: reached only after EFLAGS.ID probe.
     * Other BIOS assembly is restricted to i386 by assembler flags. */
    __asm__ volatile(".byte 0x0f,0xa2" : "=a"(*a),"=b"(*b),"=c"(*c),"=d"(*d) : "a"(n),"c"(0));
}
static void word(char *p,u32 v) {for (int i=0;i<4;i++) p[i]=(char)(v>>(8*i));}
#endif
void multi_x86_cpu_probe(void) {
    MultiCpuInfo info={0};
    info.execution_bits=sizeof(void *)*8;
#if !MULTIOS_X86_NO_CPUID
    if (has_cpuid()) {
        u32 a,b,c,d;
        info.cpuid=1;
        leaf(0,&a,&b,&c,&d);u32 maximum=a;
        word(info.vendor,b);word(info.vendor+4,d);word(info.vendor+8,c);
        if (maximum>=1) {
            leaf(1,&a,&b,&c,&d);
            multi_x86_signature(a,&info.family,&info.model,&info.stepping);
            if (d&(1u<<0)) info.features|=MULTI_CPU_FPU;
            if (d&(1u<<23)) info.features|=MULTI_CPU_MMX;
            if (d&(1u<<25)) info.features|=MULTI_CPU_SSE;
            if (d&(1u<<26)) info.features|=MULTI_CPU_SSE2;
            if (d&(1u<<15)) info.features|=MULTI_CPU_CMOV;
        }
        leaf(0x80000000u,&a,&b,&c,&d);u32 extended=a;
        if (extended>=0x80000001u) {
            leaf(0x80000001u,&a,&b,&c,&d);
            if (d&(1u<<29)) info.features|=MULTI_CPU_LONG_MODE;
        }
        if (extended>=0x80000004u) for (u32 i=0;i<3;i++) {
            leaf(0x80000002u+i,&a,&b,&c,&d);
            word(info.brand+i*16,a);word(info.brand+i*16+4,b);
            word(info.brand+i*16+8,c);word(info.brand+i*16+12,d);
        }
        /* Firmware-provided identity is diagnostic text, not an instruction. */
        for (u32 i=0;i<12;i++) if ((u8)info.vendor[i]<32 || (u8)info.vendor[i]>126) info.vendor[i]='?';
        for (u32 i=0;i<48 && info.brand[i];i++) if ((u8)info.brand[i]<32 || (u8)info.brand[i]>126) info.brand[i]='?';
    }
#endif
    multi_cpu_publish(&info);
}
