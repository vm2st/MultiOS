/* vm2 studios | t.me/v2vproj — MultiPulti i386 fault boundary */
#ifndef MULTIPULTI_PC_EXCEPTIONS_H
#define MULTIPULTI_PC_EXCEPTIONS_H
#include "lib/multi.h"
/* Matches exceptions.S, including segment values normalized to 32 bits.
 * Kernel/ring-0 only: no privilege-change SS/ESP tail is read. */
typedef struct {
    u32 gs,fs,es,ds;
    u32 edi,esi,ebp,pushad_esp,ebx,edx,ecx,eax;
    u32 vector,error,eip,cs,eflags;
} MultiFaultFrame;
void multi_pc_fault_init(void);
void multi_pc_fault_panic(const MultiFaultFrame *) __attribute__((noreturn));
void multi_pc_double_fault(void) __attribute__((noreturn));
#endif
