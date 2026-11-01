/* vm2 studios | t.me/v2vproj — signed 64-bit division without libgcc ISA drift. */
#include <stdint.h>
extern uint64_t __udivdi3(uint64_t,uint64_t),__umoddi3(uint64_t,uint64_t);
static uint64_t mag(int64_t x){uint64_t u=(uint64_t)x;return x<0?0-u:u;}
int64_t __divdi3(int64_t a,int64_t b){uint64_t q=__udivdi3(mag(a),mag(b));return (int64_t)((a<0)^(b<0)?0-q:q);}
int64_t __moddi3(int64_t a,int64_t b){uint64_t q=__umoddi3(mag(a),mag(b));return (int64_t)(a<0?0-q:q);}
