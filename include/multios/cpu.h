/* vm2 studios | t.me/v2vproj — portable CPU information, no instructions. */
#ifndef MULTIOS_CPU_H
#define MULTIOS_CPU_H
#include "multi.h"
enum { MULTI_CPU_FPU=1, MULTI_CPU_MMX=2, MULTI_CPU_SSE=4, MULTI_CPU_SSE2=8,
       MULTI_CPU_CMOV=16, MULTI_CPU_LONG_MODE=32 };
typedef struct {
    u32 execution_bits, cpuid, family, model, stepping, features;
    char vendor[13];
    char brand[49];
} MultiCpuInfo;
/* Information only: feature bits do NOT enable FPU/SIMD or change dispatch. */
int multi_cpu_publish(const MultiCpuInfo *info);
const MultiCpuInfo *multi_cpu_info(void);
void multi_cpu_report(void);
void multi_x86_signature(u32 eax, u32 *family, u32 *model, u32 *stepping);
#endif
