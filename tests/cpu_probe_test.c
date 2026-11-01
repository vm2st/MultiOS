/* vm2 studios | t.me/v2vproj — host probe sanity, not a boot test. */
#include "multios/cpu.h"
#include "platform/x86/cpu.h"
#include <assert.h>
#include <stdio.h>
Volume vol;
void out(const char *s) {(void)s;}
void number(u32 n) {(void)n;}
int main(void) {
    uint64_t before,after;
    __asm__ volatile("pushfq; popq %0":"=r"(before));
    multi_x86_cpu_probe();
    __asm__ volatile("pushfq; popq %0":"=r"(after));
    assert(!((before^after)&(1u<<21)));
    const MultiCpuInfo *cpu=multi_cpu_info();assert(cpu && cpu->execution_bits==64);
#ifdef EXPECT_NO_CPUID
    assert(!cpu->cpuid);
#else
    assert(cpu->cpuid && cpu->vendor[0]);
#endif
    puts("Host x64 CPU probe passed; ID bit restored. Not a kernel/legacy-CPU boot test.");
}
