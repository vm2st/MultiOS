/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTIOS_PC_IO_H
#define MULTIOS_PC_IO_H
#include "multios/modules.h"
#include "multios/osk.h"
#ifdef MULTIOS_PC_IO_TEST
/* Host tests only: no privileged instructions or physical port access. */
extern u32 multi_test_io_read(u16 port,u8 width);
extern void multi_test_io_write(u16 port,u32 value,u8 width);
static inline void ob(u16 p,u8 v){multi_test_io_write(p,v,1);}
static inline u8 ib(u16 p){return multi_test_io_read(p,1);}
static inline void ow(u16 p,u16 v){multi_test_io_write(p,v,2);}
static inline u16 iw(u16 p){return multi_test_io_read(p,2);}
static inline void ol(u16 p,u32 v){multi_test_io_write(p,v,4);}
static inline u32 il(u16 p){return multi_test_io_read(p,4);}
#else
static inline void ob(u16 p, u8 v) {
    __asm__ volatile("outb %0,%1" ::"a"(v), "Nd"(p));
}
static inline u8 ib(u16 p) {
    u8 v;
    __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline u16 iw(u16 p) {
    u16 v;
    __asm__ volatile("inw %1,%0" : "=a"(v) : "Nd"(p));
    return v;
}
static inline void ow(u16 p, u16 v) {
    __asm__ volatile("outw %0,%1" ::"a"(v), "Nd"(p));
}
static inline void ol(u16 p,u32 v){__asm__ volatile("outl %0,%1"::"a"(v),"Nd"(p));}
static inline u32 il(u16 p){u32 v;__asm__ volatile("inl %1,%0":"=a"(v):"Nd"(p));return v;}
#endif
#endif
