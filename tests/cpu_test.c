/* vm2 studios | t.me/v2vproj — pure signature and metadata contract tests. */
#include "multios/cpu.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;
static char result[4096];static u32 length;
void out(const char *s) {while (*s) {assert(length+1<sizeof(result));result[length++]=*s++;}result[length]=0;}
void number(u32 n) {char b[16];snprintf(b,sizeof(b),"%u",n);out(b);}
int main(void) {
    assert(!multi_cpu_info());multi_cpu_report();assert(strstr(result,"not published"));
    u32 family,model,step,seed=17;
    for (int i=0;i<10000;i++) {
        seed=seed*1664525u+1013904223u;
        multi_x86_signature(seed,&family,&model,&step);
        u32 f=(seed>>8)&15,m=(seed>>4)&15;
        assert(family==(f==15 ? f+((seed>>20)&255) : f));
        assert(model==((f==6 || f==15) ? m|(((seed>>16)&15)<<4) : m));assert(step==(seed&15));
    }
    assert(multi_cpu_publish(0)==INVALID);
    MultiCpuInfo cpu={.execution_bits=16};assert(multi_cpu_publish(&cpu)==INVALID);
    cpu.execution_bits=32;cpu.cpuid=2;assert(multi_cpu_publish(&cpu)==INVALID);
    cpu.cpuid=0;vol.mounted=1;assert(multi_cpu_publish(&cpu)==INVALID);vol.mounted=0;
    memset(cpu.vendor,'x',sizeof(cpu.vendor));memset(cpu.brand,'y',sizeof(cpu.brand));
    assert(multi_cpu_publish(&cpu)==0);assert(multi_cpu_info()->vendor[12]==0 && multi_cpu_info()->brand[48]==0);
    assert(multi_cpu_publish(&cpu)==INVALID);
    length=0;multi_cpu_report();assert(strstr(result,"baseline path active"));assert(strstr(result,"CPUID/FPU/MMX/SSE/CMOV not required"));
    puts("CPU metadata: 10000 signature cases, safe no-CPUID report, publish validation passed.");
}
