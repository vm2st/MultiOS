/* MultiOS: real FatFs bridge callbacks with deterministic failing HAL.
 * Tests adapter failure latching, NOT a filesystem or hardware model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lib/fatfs_adapter.c"
Volume vol;
static int reads,writes,flushes,upcases,mounts,upcase_error;
static int next_read,next_write,next_flush;
void *mzero(void*p,u32 n){return memset(p,0,n);}
int disk_writable(void){return 1;}
int disk_read(u32 l,u8*b){(void)l;reads++;memset(b,0x5a,512);return next_read;}
int disk_write(u32 l,const u8*b){(void)l;(void)b;writes++;return next_write;}
int disk_flush(void){flushes++;return next_flush;}
int multi_exfat_upcase(u16 c,u16*out){upcases++;*out=c;return upcase_error;}
DWORD mpff_builtin_upcase(DWORD c){return c;}
FRESULT f_mount(FATFS*f,const TCHAR*p,BYTE now){(void)f;(void)p;(void)now;mounts++;return FR_OK;}
int exfat_mount_at(u32 b,u32 n){(void)b;(void)n;mounts++;return 0;}
int fat32_mount_at(u32 b,u32 n){(void)b;(void)n;mounts++;return 0;}
static void reset(void){
 memset(&vol,0,sizeof vol);vol.mounted=vol.writable=1;vol.kind=FS_FAT32;vol.base=2048;vol.sectors=100;
 io_error=0;reads=writes=flushes=upcases=mounts=0;next_read=next_write=next_flush=upcase_error=0;
}
int main(void){
 u8 data[1024];
 for(int side=0;side<3;side++)for(int fault=0;fault<3;fault++){
  reset();int first=fault==0?NOSPACE:fault==1?IO:READONLY;
  if(side==0){next_read=first;assert(mpff_disk_read(0,data,0,2)==RES_ERROR);assert(reads==1);}
  if(side==1){next_write=first;assert(mpff_disk_write(0,data,0,2)==RES_ERROR);assert(writes==1);}
  if(side==2){next_flush=first;assert(mpff_disk_ioctl(0,CTRL_SYNC,0)==RES_ERROR);assert(flushes==1);}
  int a=reads,b=writes,c=flushes;
  next_read=next_write=next_flush=0;
  assert(mpff_disk_read(0,data,0,1)==RES_ERROR);
  assert(mpff_disk_write(0,data,0,1)==RES_ERROR);
  assert(mpff_disk_ioctl(0,CTRL_SYNC,0)==RES_ERROR);
  assert(reads==a&&writes==b&&flushes==c);assert(error(FR_OK)==first);
  vol.kind=FS_EXFAT;upcase_error=BAD;(void)ff_wtoupper('a');assert(io_error==first&&upcases==0);
  assert(finish(0)==first);assert(mounts==1); /* unregister only, NO remount */
 }
 reset();vol.kind=FS_EXFAT;upcase_error=BAD;(void)ff_wtoupper('a');assert(io_error==BAD&&upcases==1);
 (void)ff_wtoupper('b');assert(upcases==1&&io_error==BAD);
 assert(mpff_disk_write(0,data,0,1)==RES_ERROR&&writes==0);
 reset();assert(mpff_disk_read(0,data,0,2)==RES_OK&&reads==2);
 assert(mpff_disk_write(0,data,0,2)==RES_OK&&writes==2);
 assert(mpff_disk_ioctl(0,CTRL_SYNC,0)==RES_OK&&flushes==1);
 assert(mpff_disk_read(0,data,100,1)==RES_PARERR&&reads==2);
 assert(mpff_disk_write(0,data,99,2)==RES_PARERR&&writes==2);
 assert(mpff_disk_read(0,0,0,1)==RES_PARERR&&reads==2);
 assert(mpff_disk_write(0,0,0,1)==RES_PARERR&&writes==2);
 assert(finish(0)==0&&mounts==2);
 puts("PASS real FatFs bridge: 9 first-error cases, no read/write/flush/upcase after failure, no success remount after failure, bounds/null guards, clean I/O.");
}
