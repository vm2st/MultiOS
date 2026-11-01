/* MultiOS / MultiPulti: bounded RO-view cache, NEVER a write-back cache.
 * The owner must clear before writable views, remounts and failures.
 * Kept ABOVE the block/undo layer: persistence read-back must stay uncached. */
#ifndef MULTIOS_READ_CACHE_H
#define MULTIOS_READ_CACHE_H
#include "multi.h"
#define MULTI_READ_CACHE_SETS 128u
#define MULTI_READ_CACHE_WAYS 4u
typedef struct {u32 lba,age;u8 valid,data[512];} MultiReadCacheEntry;
typedef struct {MultiReadCacheEntry entry[512];u32 clock,hits,misses,errors;} MultiReadCache;
static inline void multi_read_cache_clear(MultiReadCache *c){for(u32 i=0;i<512;i++)c->entry[i].valid=0;c->clock=0;}
static inline int multi_read_cache_get(MultiReadCache *c,void *ctx,int (*read)(void*,u32,u8*),u32 lba,u8 *out){
 if(!c||!read||!out)return INVALID;
 if(++c->clock==0){multi_read_cache_clear(c);c->clock=1;}
 u32 base=(lba%MULTI_READ_CACHE_SETS)*MULTI_READ_CACHE_WAYS;MultiReadCacheEntry *slot=&c->entry[base];
 for(u32 i=0;i<MULTI_READ_CACHE_WAYS;i++){
  MultiReadCacheEntry *e=&c->entry[base+i];
  if(e->valid&&e->lba==lba){e->age=c->clock;c->hits++;for(u32 j=0;j<512;j++)out[j]=e->data[j];return 0;}
  if(!e->valid||(slot->valid&&e->age<slot->age))slot=e;
 }
 c->misses++;int r=read(ctx,lba,out);
 if(r){c->errors++;multi_read_cache_clear(c);return r<0?r:IO;}
 slot->lba=lba;slot->age=c->clock;for(u32 j=0;j<512;j++)slot->data[j]=out[j];slot->valid=1;return 0;
}
#endif
