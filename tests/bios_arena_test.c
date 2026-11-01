/* vm2 studios | t.me/v2vproj — allocator fragmentation/overflow/reset model. */
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
void *mpntfs_malloc(size_t),*mpntfs_calloc(size_t,size_t),*mpntfs_realloc(void*,size_t);
void mpntfs_free(void*),mpntfs_reset(void);
static uint32_t rng=7;
static uint32_t next(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){void*p[128]={0};size_t sizes[128]={0};mpntfs_reset();
 for(unsigned n=0;n<50000;n++){unsigned k=next()%128;unsigned char tag=k+1;
  if(p[k])for(size_t i=0;i<sizes[k];i++)assert(((unsigned char*)p[k])[i]==tag);
  if(next()&1){mpntfs_free(p[k]);p[k]=0;sizes[k]=0;}else{size_t s=next()%32768+1;void*q=mpntfs_realloc(p[k],s);assert(q&&((uintptr_t)q&15)==0);for(size_t i=0;i<sizes[k]&&i<s;i++)assert(((unsigned char*)q)[i]==tag);p[k]=q;sizes[k]=s;memset(q,tag,s);}}
 for(unsigned i=0;i<128;i++)mpntfs_free(p[i]);
 void*q=mpntfs_malloc(23*1024*1024);assert(q);mpntfs_free(q);
 assert(!mpntfs_malloc(SIZE_MAX));assert(!mpntfs_calloc(SIZE_MAX,2));
 q=mpntfs_calloc(16384,1);assert(q);for(size_t i=0;i<16384;i++)assert(!((unsigned char*)q)[i]);mpntfs_reset();assert(mpntfs_malloc(23*1024*1024));
 puts("PASS BIOS arena: 50000 randomized allocation/reallocation/free operations, preserved bytes, coalescing, alignment, overflow and reset; host model only.");}
