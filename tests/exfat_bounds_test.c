/* vm2 studios | t.me/v2vproj — hostile API values, no disk reads expected */
#include "multi.h"
#include "fs_backend.h"
#include <assert.h>
#include <stdio.h>
static int reads;
int disk_read(u32 l,u8 *b){(void)l;(void)b;reads++;return IO;}
int disk_write(u32 l,const u8 *b){(void)l;(void)b;assert(0);return IO;}
int disk_flush(void){return 0;}
u32 disk_sectors(void){return 65536;}
int disk_writable(void){return 1;}
void putch(char c){(void)c;}
void out(const char *s){(void)s;}
int main(void){
 vol.cluster_bytes=4096;vol.clusters=16;vol.root=2;
 Stream s={2,4096,4096,3,0};Entry e;u32 p=0xfffffff0u;
 assert(fs_next(s,&p,&e)==BAD && reads==0);
 p=3;assert(fs_next(s,&p,&e)==BAD && reads==0);
 p=4096;assert(fs_next(s,&p,&e)==0 && reads==0);
 assert(fs_next(s,0,&e)==INVALID && fs_next(s,&p,0)==INVALID);
 s.size=4095;assert(fs_next(s,&p,&e)==BAD);
 s.size=4096;s.valid=0;assert(fs_next(s,&p,&e)==BAD);
 s=(Stream){0,0,0,1,0};assert(fs_read(s,0,0,0)==0);
 s.size=1;assert(fs_read(s,0,0,1)==INVALID);
 s.valid=2;assert(fs_read(s,0,0,0)==BAD);
 vol.mounted=1;vol.writable=1;vol.clusters=65537;
 assert(fs_unlock()==UNSUP && !vol.writable && reads==0);
 assert(fs_put("/x",0,1,1,0)==INVALID);
 assert(fs_put("/x",0,0,4,0)==INVALID);
 assert(fs_put("/x",0,0,9,0)==INVALID);
 assert(exfat_put("/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a/a",0,0,3,0)==UNSUP && reads==0);
 puts("exFAT bounds: cursor overflow/alignment, directory size/VDL, null/zero read, write-audit capacity, unsupported mode/source and mkdir depth passed without disk I/O.");
}
