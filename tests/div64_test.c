/* vm2 studios | t.me/v2vproj — native reference for i386 arithmetic helpers. */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
uint64_t __udivdi3(uint64_t,uint64_t);uint64_t __umoddi3(uint64_t,uint64_t);
static uint64_t state=0x6673343864697636ULL;
static uint64_t rnd(void){state^=state<<13;state^=state>>7;state^=state<<17;return state;}
static void check(uint64_t n,uint64_t d){assert(__udivdi3(n,d)==n/d);assert(__umoddi3(n,d)==n%d);}
int main(void){uint64_t edge[]={0,1,2,0xffffffffULL,0x100000000ULL,0x7fffffffffffffffULL,0x8000000000000000ULL,UINT64_MAX};for(unsigned i=0;i<8;i++)for(unsigned j=1;j<8;j++)check(edge[i],edge[j]);for(unsigned i=0;i<100000;i++){uint64_t n=rnd(),d=rnd()|1;check(n,d);}puts("PASS unsigned 64-bit division/remainder: 100056 pairs against native x64 division.");}
