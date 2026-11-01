/* vm2 studios | t.me/v2vproj — read-only VFS probe with write trap. */
#define _POSIX_C_SOURCE 200809L
#include "multios/hal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static FILE*file;static u32 sectors;
static int rd(void*x,u32 l,u8*b){(void)x;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fread(b,1,512,file)==512?0:IO;}
static int wr(void*x,u32 l,const u8*b){(void)x;(void)l;(void)b;assert(!"read path attempted disk write");return IO;}
static int syncdisk(void*x){(void)x;return 0;}
static void put(void*x,char c){(void)x;(void)c;}static int get(void*x){(void)x;return -1;}
int multi_osk_process(int k){return k;}
int main(int argc,char**argv){assert(argc==5);file=fopen(argv[1],"rb");assert(file);assert(!fseeko(file,0,SEEK_END));off_t bytes=ftello(file);assert(bytes>0&&bytes%512==0&&bytes/512<=UINT32_MAX);sectors=(u32)(bytes/512);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="RO probe",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=0,.read=rd,.write=wr,.flush=syncdisk},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount());MultiFile64 f;int r=fs_open64(argv[2],&f);if(r){fprintf(stderr,"open failed %d\n",r);return 2;}
 uint64_t off=strtoull(argv[3],0,0);unsigned long count=strtoul(argv[4],0,0);assert(count<=1024*1024);u8*b=malloc(count?count:1);assert(b);
 r=fs_read64(&f,off,b,(u32)count);if(r){fprintf(stderr,"read failed %d\n",r);return 3;}
 fprintf(stderr,"size=%llu valid=%llu\n",(unsigned long long)f.size,(unsigned long long)f.valid);assert(fwrite(b,1,count,stdout)==count);
 u8 sentinel=0x5a;assert(fs_read64(&f,f.size+1,&sentinel,1)<0&&sentinel==0x5a);free(b);fclose(file);return 0;}
