/* vm2 studios | t.me/v2vproj — freestanding i386 unsigned 64-bit division.
 * No libgcc dependency, no instruction newer than 386. */
#include <stdint.h>
static uint64_t divmod(uint64_t n,uint64_t d,uint64_t*rem){
 if(!d)__builtin_trap();uint64_t q=0,r=0;
 for(unsigned i=64;i;i--){unsigned carry=(unsigned)(r>>63);r=(r<<1)|((n>>(i-1))&1);
  if(carry||r>=d){r-=d;q|=UINT64_C(1)<<(i-1);}}
 if(rem)*rem=r;return q;
}
uint64_t __udivdi3(uint64_t n,uint64_t d){return divmod(n,d,0);}
uint64_t __umoddi3(uint64_t n,uint64_t d){uint64_t r;divmod(n,d,&r);return r;}
