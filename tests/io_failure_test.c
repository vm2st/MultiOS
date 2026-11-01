/* vm2 studios | t.me/v2vproj — HAL fail-closed write state */
#include "multios/hal.h"
#include <assert.h>
#include <stdio.h>
Volume vol;
static int result,writes;
static int rd(void *c,u32 l,u8 *b){(void)c;(void)l;(void)b;return result;}
static int wr(void *c,u32 l,const u8 *b){(void)c;(void)l;(void)b;writes++;return result;}
static int flush(void *c){(void)c;return result;}
static void emit(void *c,char x){(void)c;(void)x;}
static int key(void *c){(void)c;return -1;}
int main(void){
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="failure test",.architecture="host",
  .block={.sectors=2,.sector_bytes=512,.writable=1,.read=rd,.write=wr,.flush=flush},
  .console={.putc=emit,.getc=key}};
 assert(multi_hal_bind(&h)==0);u8 b[512];
 for(int kind=0;kind<3;kind++)for(int failure=0;failure<2;failure++){
  vol.mounted=1;vol.writable=1;vol.poisoned=0;result=failure?7:IO;
  int r=kind==0?disk_read(0,b):kind==1?disk_write(0,b):disk_flush();
  assert(r==IO && vol.poisoned && !vol.writable);
  int before=writes;result=0;assert(disk_write(0,b)==READONLY && writes==before);
 }
 vol.mounted=1;vol.writable=0;vol.poisoned=0;result=IO;
 assert(disk_read(0,b)==IO && !vol.poisoned); /* RO read error does not invent a write failure */
 assert(disk_flush()==IO && vol.poisoned && !vol.writable);
 puts("HAL I/O failure: read/write/flush negative and illegal-positive provider errors lock RW volume; subsequent raw writes blocked; RO read remains RO.");
}
