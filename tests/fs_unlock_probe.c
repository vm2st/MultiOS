/* Real VFS/backend unlock with failed read/flush callbacks. File copies only. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static unsigned long reads,flushes,writes,cut;static int side,armed,hit;
static int guarded_read(void*x,u32 l,u8*b){if(armed&&++reads==cut&&side==0){hit=1;return IO;}return rd(x,l,b);}
static int guarded_write(void*x,u32 l,const u8*b){(void)x;(void)l;(void)b;writes++;return IO;}
static int guarded_flush(void*x){(void)x;if(armed&&++flushes==cut&&side==1){hit=1;return IO;}return 0;}
int main(int argc,char**argv){
 assert(argc==4);side=atoi(argv[2]);cut=strtoul(argv[3],0,10);file=fopen(argv[1],"rb");assert(file);
 assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="unlock fault probe",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=guarded_read,.write=guarded_write,.flush=guarded_flush},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount());armed=1;
 int r=fs_unlock();
 fprintf(stderr,"unlock result=%d writable=%d poisoned=%d reads=%lu flushes=%lu hit=%d\n",r,vol.writable,vol.poisoned,reads,flushes,hit);
 if(cut){assert(hit);assert(r<0&&!vol.writable);}else assert(!r&&vol.writable&&!vol.poisoned);
 assert(!writes);printf("PASS unlock: reads=%lu flushes=%lu hit=%d\n",reads,flushes,hit);fclose(file);return 0;
}
