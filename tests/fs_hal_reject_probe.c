/* MultiOS — storage integration must roll back even a misbehaving FS module. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include "multios/storage_tx.h"
#include "multios/undo.h"
#include <string.h>
static u32 writes;
static int tx_write(void*x,u32 l,const u8*b){(void)x;writes++;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int tx_flush(void*x){(void)x;return fflush(file)?IO:0;}
static void mountdisk(void){vol.mounted=vol.writable=0;MultiHal h={.abi=MULTIOS_HAL_ABI,.name="transaction contract test",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=tx_write,.flush=tx_flush},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static u8 old[MULTIOS_UNDO_MAX_RECORDS+1][512],changed[512],check[512];
int main(int argc,char **argv){assert(argc==2);file=fopen(argv[1],"r+b");assert(file&&!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();u32 base=vol.base;
 assert(!rd(0,sectors-1,check));u32 journal_start=(u32)check[24]|((u32)check[25]<<8)|((u32)check[26]<<16)|((u32)check[27]<<24);
 assert(!rd(0,journal_start,check));u32 budget=(u32)check[44]|((u32)check[45]<<8)|((u32)check[46]<<16)|((u32)check[47]<<24);
 assert(budget&&budget<=MULTIOS_UNDO_MAX_RECORDS&&budget<vol.sectors);
 for(u32 i=0;i<=budget;i++)assert(!rd(0,base+i,old[i]));
 for(int op=0;op<6;op++){
  assert(!multi_storage_begin());memcpy(changed,old[0],512);changed[99]^=0x5a;
  assert(!disk_write(base,changed));
  int r=op==0?disk_write(sectors,changed):op==1?disk_write(base,0):op==2?disk_read(sectors,check):op==3?disk_read(base,0):op==4?multi_storage_read(sectors,check):multi_storage_read(base,0);
  assert(r<0);u32 before=writes;
  assert(disk_write(base,changed)<0&&writes==before);
  int result=multi_storage_finish(0);
  printf("HAL rejected op=%d with %d; ignored-error finish=%d (must fail)\n",op,r,result);fflush(stdout);
  assert(result==r&&!vol.mounted&&!vol.writable&&!multi_storage_blocked());
  assert(writes>before); /* recovery, not another application write */
  assert(!rd(0,base,check)&&!memcmp(check,old[0],512));mountdisk();
 }
 assert(!multi_storage_begin());
 for(u32 i=0;i<budget;i++){memcpy(changed,old[i],512);changed[99]^=0xa5;assert(!disk_write(base+i,changed));}
 assert(disk_write(base+budget,old[budget])==NOSPACE);u32 before=writes;
 assert(disk_write(base,old[0])==READONLY&&writes==before); /* HAL already poisoned */
 /* A module incorrectly reports success despite the failed disk_write. */
 assert(multi_storage_finish(0)==NOSPACE&&!vol.mounted&&!vol.writable&&!multi_storage_blocked());
 for(u32 i=0;i<=budget;i++)assert(!rd(0,base+i,check)&&!memcmp(check,old[i],512));
 mountdisk();assert(!multi_storage_begin()&&!disk_write(base,old[0])&&!multi_storage_finish(0));
 assert(!fs_put("/KEEP.TXT",(const u8*)"KEEP!",5,0,0)&&!disk_flush());fclose(file);
 puts("PASS HAL preflight rejection: six ignored read/write errors roll back original sectors; swallowed journal overflow also rolls back; remount and normal RW work");return 0;
}
