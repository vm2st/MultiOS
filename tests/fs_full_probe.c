/* Real FAT32/exFAT/NTFS engines on a disposable, nearly full RAM-backed image.
 * Capacity failure must roll back ALL non-journal bytes, not just file data. */
#include "multios/hal.h"
#include "multios/storage_tx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static u8 *image,*snapshot;static u32 sectors,journal,reads,writes;
static int rd(void*x,u32 l,u8*b){(void)x;assert(l<sectors);reads++;memcpy(b,image+(size_t)l*512,512);return 0;}
static int wr(void*x,u32 l,const u8*b){(void)x;assert(l<sectors);writes++;memcpy(image+(size_t)l*512,b,512);return 0;}
static int flush(void*x){(void)x;return 0;}
static void put(void*x,char c){(void)x;(void)c;}static int get(void*x){(void)x;return -1;}
int multi_osk_process(int k){return k;}
static void reopen(void){
 memset(&vol,0,sizeof vol);vol.kind=-1;
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="near-full RAM file image",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=wr,.flush=flush},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());
}
static u32 le32(const u8*p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f);assert(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);assert(size>0&&size%512==0);
 image=malloc(size);snapshot=malloc(size);assert(image&&snapshot);assert(fread(image,1,size,f)==(size_t)size);fclose(f);
 sectors=size/512;journal=le32(image+(size_t)(sectors-1)*512+24);assert(journal&&journal<sectors);
 reopen();int kind=vol.kind;assert(!fs_put("/KEEP.TXT",(const u8*)"PRESERVE",8,0,0));
 assert(!fs_put("/FILLER.BIN",0,0,0,0));uint64_t total=0;u32 freeclusters;unsigned fills=0;
 static u8 filler[262144];memset(filler,0x5b,sizeof filler);
 for(;;){assert(!fs_free(&freeclusters));uint64_t available=(uint64_t)freeclusters*vol.cluster_bytes;
  if(available<65536)break;uint64_t grow=available-32768;if(grow>262144)grow=262144;
  /* NTFS truncate can grow a sparse file without consuming clusters.
   * Nonzero writes exercise actual device-space exhaustion on all engines. */
  int r=fs_put("/FILLER.BIN",filler,(u32)grow,2,0);if(r){fprintf(stderr,"fill failed kind=%d result=%d free=%u total=%llu\n",kind,r,freeclusters,(unsigned long long)total);abort();}
  total+=grow;assert(++fills<2000);
 }
 assert(!disk_flush());memcpy(snapshot,image,size);
 u8 *payload=malloc(262144);assert(payload);memset(payload,0xa7,262144);
 u32 keep_size=8;
 for(unsigned op=0;op<4;op++){
  int r;
  if(op==2&&kind==FS_NTFS){
   /* Sparse NTFS growth is legal even on a nearly full volume. The following
    * nonzero write must fail when it needs to materialize physical clusters. */
   assert(!fs_resize64("/KEEP.TXT",262144));keep_size=262144;
   MultiFile64 sparse;u8 zero[4096];assert(!fs_open64("/KEEP.TXT",&sparse)&&sparse.size==keep_size);
   for(u32 off=8;off<keep_size;){u32 n=keep_size-off;if(n>sizeof zero)n=sizeof zero;
    assert(!fs_read64(&sparse,off,zero,n));for(u32 i=0;i<n;i++)assert(!zero[i]);off+=n;}
   memcpy(snapshot,image,size);
   r=fs_write64("/KEEP.TXT",0,payload,262144);
  }else r=op==0?fs_put("/FAIL.BIN",payload,262144,0,0):op==1?fs_put("/KEEP.TXT",payload,262144,2,0):op==2?fs_resize64("/KEEP.TXT",262144):fs_put("/COPY.BIN",0,0,4,"/FILLER.BIN");
  fprintf(stderr,"full kind=%d op=%u result=%d free_clusters=%u\n",kind,op,r,freeclusters);
  assert(r==NOSPACE&&!vol.mounted&&!vol.writable&&!multi_storage_blocked());
  assert(!memcmp(snapshot,image,(size_t)journal*512));
  assert(!memcmp(snapshot+(size_t)(sectors-2)*512,image+(size_t)(sectors-2)*512,1024));
  reopen();MultiFile64 v;u8 keep[8];assert(!fs_open64("/KEEP.TXT",&v)&&v.size==keep_size);
  assert(!fs_read64(&v,0,keep,8)&&!memcmp(keep,"PRESERVE",8));
  assert(fs_open64("/FAIL.BIN",&v)==NOTFOUND&&fs_open64("/COPY.BIN",&v)==NOTFOUND);
 }
 f=fopen(argv[2],"wb");assert(f);assert(fwrite(image,1,size,f)==(size_t)size);assert(!fclose(f));
 printf("PASS near-full FS kind=%d: %u fill operations, %u free clusters, 4 ENOSPC rollbacks (NTFS sparse materialization, not sparse growth), complete non-journal byte comparison, remount and preserved data; reads=%u writes=%u.\n",kind,fills,freeclusters,reads,writes);
 free(payload);free(snapshot);free(image);return 0;
}
