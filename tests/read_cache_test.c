#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "multios/read_cache.h"
static MultiReadCache c;
static u32 reads,epoch,rng=7;static int fail;
static u32 random32(void){rng=rng*1664525u+1013904223u;return rng;}
static int raw(void *ctx,u32 lba,u8 *out){assert(ctx==&reads);reads++;for(u32 j=0;j<512;j++)out[j]=(u8)(lba*37+j*13+epoch);return fail;}
static int get(u32 lba,u8 *out){return multi_read_cache_get(&c,&reads,raw,lba,out);}
int main(void){
 u8 out[512];assert(multi_read_cache_get(0,0,raw,0,out)==INVALID);
 assert(multi_read_cache_get(&c,0,0,0,out)==INVALID);assert(multi_read_cache_get(&c,0,raw,0,0)==INVALID);
 for(u32 i=0;i<100000;i++){
  u32 lba=random32()%2048;if(!(i%79)){epoch++;multi_read_cache_clear(&c);}
  assert(get(lba,out)==0);for(u32 j=0;j<512;j++)assert(out[j]==(u8)(lba*37+j*13+epoch));
 }
 multi_read_cache_clear(&c);u32 n=reads;
 for(u32 i=0;i<4;i++)assert(get(i*128,out)==0);
 for(u32 i=0;i<4;i++)assert(get(i*128,out)==0);assert(reads==n+4);
 assert(get(512,out)==0);n=reads;assert(get(0,out)==0&&reads==n+1);
 for(int f=-1;f<=1;f+=2){fail=f;assert(get(0xffffffffu,out)==(f<0?f:IO));fail=0;n=reads;assert(get(0,out)==0&&reads==n+1);}
 c.clock=0xffffffffu;n=reads;assert(get(0,out)==0&&c.clock==1&&reads==n+1);
 puts("PASS: RO-view cache 100000 randomized reads, invalidation/epoch, LRU collisions, partial failed read, positive error, wrap, argument guards.");
}
