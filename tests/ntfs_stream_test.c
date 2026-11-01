/* vm2 studios | t.me/v2vproj — test-only offset reads through public VFS. */
#include "fs_backend.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *image;static u32 sectors;
int disk_read(u32 l,u8*b){if(l>=sectors||fseek(image,(long)l*512,SEEK_SET))return IO;return fread(b,1,512,image)==512?0:IO;}
int disk_write(u32 l,const u8*b){(void)l;(void)b;assert(0);return IO;}
int disk_flush(void){assert(0);return IO;}
u32 disk_sectors(void){return sectors;}
int disk_writable(void){return 1;}
void out(const char*s){(void)s;}void putch(char c){(void)c;}
int main(int argc,char**argv){assert(argc==2);image=fopen(argv[1],"rb");assert(image);assert(!fseek(image,0,SEEK_END));long n=ftell(image);assert(n>0&&n%512==0);sectors=n/512;assert(!fs_mount());Entry e;assert(!fs_lookup(fs_root(),"split.bin",&e));
 u8 b[8192];unsigned checks=0;
 /* Includes a single unaligned request spanning the DATA-extent boundary. */
 const u32 offsets[]={0,1,4093,4096,80*4096-4097,80*4096-1,80*4096,159*4096+1,655360};
 const u32 lengths[]={0,1,2,511,512,4096,8192};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(*offsets);i++)for(unsigned j=0;j<sizeof(lengths)/sizeof(*lengths);j++){
  u32 o=offsets[i],take=lengths[j];memset(b,0xa5,sizeof(b));int r=fs_read(e.data,o,b,take);
  if(o>655360||take>655360-o){assert(r<0);for(unsigned k=0;k<sizeof(b);k++)assert(b[k]==0xa5);}else{
   assert(!r);for(u32 k=0;k<take;k++)assert(b[k]==((o+k)/4096)%251);for(u32 k=take;k<sizeof(b);k++)assert(b[k]==0xa5);
  }checks++;
 }
 assert(!fs_lookup(fs_root(),"resident.txt",&e));assert(!fs_read(e.data,3,b,7)&&!memcmp(b,"ident N",7));assert(fs_read(e.data,14,b,1)<0);assert(fs_read(e.data,0,NULL,1)<0);
 fclose(image);printf("PASS %u offset/bounds reads, including cross-extent unaligned read; resident snapshot bounds.\n",checks);return 0;
}
