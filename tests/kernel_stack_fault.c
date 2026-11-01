/* vm2 studios | t.me/v2vproj — TEST IMAGE ONLY, never linked in production.
 * Change the global guard while a protected frame is live, then return.
 * This tests the compiler-generated canary epilogue without an undefined C
 * out-of-bounds write or an assumption about GCC stack layout. */
#include "lib/multi.h"
extern u32 __stack_chk_guard;
int __attribute__((noinline)) multi_stack_fault_test(void){
    volatile char buffer[32];
    buffer[0]=1;
    __asm__ volatile(""::"r"(buffer):"memory");
    __stack_chk_guard^=1;
    return buffer[0];
}
