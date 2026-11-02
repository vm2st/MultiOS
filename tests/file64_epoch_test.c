/* vm2 studios | t.me/v2vproj — metadata mutation invalidates extended views. */
#include "fs_backend.h"
#include "unicode.h"
#include <assert.h>
#include <stdio.h>
static int result,calls,readcalls;
static int mount_result,read_result,next_result,free_result,mount_bad,next_stall;
int disk_read(u32 l,u8*b){(void)l;mzero(b,512);return 0;}
int disk_write(u32 l,const u8*b){(void)l;(void)b;assert(0);return IO;}
int disk_flush(void){return 0;}u32 disk_sectors(void){return 128;}int disk_writable(void){return 1;}
void out(const char*s){(void)s;}void putch(char c){(void)c;}
static int probe(const u8*b){(void)b;return 1;}static int priority(u8 t){(void)t;return 100;}
static int mount(u32 b,u32 n){vol.base=b;vol.sectors=n;if(mount_bad)vol.base++;return mount_result;}
static Stream root(void){Stream s={.first=2,.flags=ATTR_DIR,.root=1};return s;}
static int next(Stream s,u32*c,Entry*e){(void)s;if(next_result)return next_result;if(*c)return 0;mzero(e,sizeof(*e));mcopy(e->name,"file",5);e->ascii=1;e->data=(Stream){.first=3,.size=1,.valid=1};if(!next_stall)*c=1;return 1;}
static int read32(Stream s,u32 o,u8*b,u32 n){(void)s;(void)o;if(n)*b='X';return read_result;}
static int read64(const MultiFile64*f,uint64_t o,u8*b,u32 n){(void)f;(void)o;readcalls++;if(n)*b='X';return read_result;}
static int freecount(u32*n){*n=1;return free_result;}static int unlock(void){vol.writable=1;return 0;}
static int put(const char*p,const u8*b,u32 n,int m,const char*s){(void)p;(void)b;(void)n;(void)m;(void)s;calls++;return result;}
static int writeat(const char*p,uint64_t o,const u8*b,u32 n){(void)o;return put(p,b,n,0,0);}
static int resizefile(const char*p,uint64_t n){(void)n;return put(p,0,0,0,0);}
static int removefile(const char*p,int d){(void)p;(void)d;calls++;return result;}
static int renamefile(const char*p,const char*n){(void)p;(void)n;calls++;return result;}
static int snext(const char*p,u32*c,MultiNamedStream*i){(void)p;(void)c;(void)i;return 0;}
static int sread(const char*p,const char*s,uint64_t o,u8*b,u32 n){(void)p;(void)s;return read64(0,o,b,n);}
static int schange(const char*p,const char*s,int op,uint64_t o,const u8*b,u32 n){(void)s;(void)op;(void)o;return put(p,b,n,0,0);}
const MultiFsDriver multi_fs_fat32={.abi=MULTIOS_FS_ABI,.size=sizeof(MultiFsDriver),.kind=FS_FAT32,.id="mock",.name="TEST ONLY",.capabilities=MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_RANDOM_IO|MULTIOS_FS_NAMED_STREAMS|MULTIOS_FS_HARDLINKS,.probe=probe,.partition_priority=priority,.mount=mount,.root=root,.next=next,.read=read32,.read64=read64,.name_equal=multi_default_equal,.free=freecount,.unlock=unlock,.put=put,.remove=removefile,.rename=renamefile,.write_at=writeat,.resize=resizefile,.stream_next=snext,.stream_read=sread,.stream_change=schange,.link=renamefile};
int main(void){u8 b;MultiFile64 f;assert(!fs_mount());
 for(int fail=0;fail<2;fail++)for(int op=0;op<7;op++){
  assert(!fs_unlock());assert(!fs_open64("/file",&f));assert(!fs_read64(&f,0,&b,1)&&b=='X');result=fail?IO:0;int before=calls;
  int r=op==0?fs_put("/file",(const u8*)"x",1,0,NULL):op==1?fs_remove("/file",0):op==2?fs_rename("/file","other"):op==3?fs_write64("/file",0,(const u8*)"x",1):op==4?fs_resize64("/file",2):op==5?fs_stream_change("/file","meta",MULTIOS_STREAM_CREATE,0,(const u8*)"x",1):fs_link("/file","/other");assert(r==result&&calls==before+1);
  before=readcalls;assert(fs_read64(&f,0,&b,1)==BAD&&readcalls==before);
 }
 /* Denied/non-dispatched mutation must not turn a valid RO view into a new one. */
 assert(!fs_open64("/file",&f));vol.writable=0;int before=calls;assert(fs_remove("/file",0)==READONLY&&calls==before);assert(!fs_read64(&f,0,&b,1));
 puts("PASS mock VFS: put/remove/rename/write-at/resize/ADS/hardlink success + partial-failure paths invalidate old views before dispatch; denied operations never dispatch; read-only view remains usable.");return 0;}
