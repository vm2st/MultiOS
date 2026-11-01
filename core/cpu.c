/* vm2 studios | t.me/v2vproj — metadata only; architecture instructions stay in BSP. */
#include "multios/cpu.h"
static MultiCpuInfo cpu;
static int known;
int multi_cpu_publish(const MultiCpuInfo *info) {
    if (!info || known || vol.mounted || (info->execution_bits!=32 && info->execution_bits!=64) || info->cpuid>1)
        return INVALID;
    cpu=*info;
    cpu.vendor[12]=0; cpu.brand[48]=0;
    known=1; return 0;
}
const MultiCpuInfo *multi_cpu_info(void) { return known ? &cpu : 0; }
void multi_x86_signature(u32 eax,u32 *family,u32 *model,u32 *stepping) {
    u32 f=(eax>>8)&15, m=(eax>>4)&15;
    *family=f==15 ? f+((eax>>20)&255) : f;
    *model=(f==6 || f==15) ? m|(((eax>>16)&15)<<4) : m;
    *stepping=eax&15;
}
void multi_cpu_report(void) {
    if (!known) { out("CPU information not published by this platform (host harness is not a booted CPU).\n"); return; }
    out("MultiPulti CPU | vm2 studios | t.me/v2vproj\nExecution: ");number(cpu.execution_bits);out("-bit\n");
    out(cpu.execution_bits==32 ? "Required ISA: i386 integer baseline; CPUID/FPU/MMX/SSE/CMOV not required.\n" :
        "Required ISA: x86-64 UEFI ABI; not the BIOS i386 target.\n");
    out("CPUID: ");out(cpu.cpuid ? "available\n" : "unavailable or disabled; baseline path active\n");
    if (cpu.cpuid) {
        out("Vendor: ");out(cpu.vendor);out("\nBrand: ");out(cpu.brand[0]?cpu.brand:"not provided");
        out("\nFamily: ");number(cpu.family);out(" Model: ");number(cpu.model);out(" Stepping: ");number(cpu.stepping);out("\n");
        const char *names[]={"FPU","MMX","SSE","SSE2","CMOV","long-mode"};
        out("Advertised features (not enabled/used by this report):");
        for (int i=0;i<6;i++) if (cpu.features&(1u<<i)) {out(" ");out(names[i]);}
        out("\n");
    }
    out("CPU compatibility is not motherboard, firmware or storage compatibility.\n");
}
