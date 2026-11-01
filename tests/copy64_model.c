/* vm2 studios | t.me/v2vproj — synthetic 64-bit VFS, NOT an on-disk FS test. */
#include "multios/copy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;static uint64_t source_size=5ULL*1024*1024*1024+123,stage_size;
static int stage_exists,published,seen_high;static u32 epoch=1,appends;
void out(const char*s){(void)s;}void putch(char c){(void)c;}
int fs_open64(const char*p,MultiFile64*f){
 int source=!strcmp(p,"/SOURCE");if(!source&&(!stage_exists||strcmp(p,published?"/FINAL":"/MC000001.TMP")))return NOTFOUND;
 *f=(MultiFile64){.data={.first=source?10:11},.size=source?source_size:stage_size,.valid=source?source_size:stage_size,.epoch=epoch,.kind=vol.kind};return 0;
}
int fs_read64(const MultiFile64*f,uint64_t o,u8*b,u32 n){assert(f->epoch==epoch&&o<=f->size&&n<=f->size-o);if(o>UINT32_MAX)seen_high=1;for(u32 i=0;i<n;i++)b[i]=(u8)((o+i)>>32);return 0;}
int fs_parent(const char*p,Stream*s,char*name){(void)p;*s=(Stream){.first=1,.root=1};strcpy(name,"FINAL");return 0;}
int fs_names_equal(const char*a,const char*b){return same(a,b);}
int fs_resolve(const char*p,Stream*s,Entry*e,int*r){(void)p;(void)s;(void)e;(void)r;return UNSUP;}
int fs_put(const char*p,const u8*b,u32 n,int m,const char*src){(void)src;assert(!strcmp(p,"/MC000001.TMP"));if(m==0){assert(!stage_exists&&!n);stage_exists=1;}else{assert(m==2&&stage_exists&&n<=MULTIOS_COPY_CHUNK);for(u32 i=0;i<n;i++)assert(b[i]==(u8)((stage_size+i)>>32));stage_size+=n;appends++;}epoch++;return 0;}
int fs_rename(const char*p,const char*n){assert(!strcmp(p,"/MC000001.TMP")&&!strcmp(n,"FINAL")&&stage_size==source_size);published=1;epoch++;return 0;}
int main(void){MultiCopyResult c;vol=(Volume){.kind=FS_FAT32,.mounted=1,.writable=1};assert(fs_copy_staged("/SOURCE","/FINAL",0,&c,0,0)==UNSUP&&!stage_exists&&!appends);
 vol.kind=FS_NTFS;assert(!fs_copy_staged("/SOURCE","/FINAL",0,&c,0,0)&&c.published&&c.completed==source_size&&seen_high&&appends>655360);
 puts("PASS synthetic VFS: verified 5 GiB+123 byte copy, offsets beyond 32 bits, view epochs; FAT32 >4GiB rejected before mutation. NOT a real NTFS/exFAT disk test.");return 0;}

u32 fs_capabilities(void){return 0;}
int fs_stream_next(const char*p,u32*c,MultiNamedStream*i){(void)p;(void)c;(void)i;return UNSUP;}
int fs_stream_read(const char*p,const char*s,uint64_t o,u8*b,u32 n){(void)p;(void)s;(void)o;(void)b;(void)n;return UNSUP;}
int fs_stream_change(const char*p,const char*s,int op,uint64_t o,const u8*b,u32 n){(void)p;(void)s;(void)op;(void)o;(void)b;(void)n;return UNSUP;}
