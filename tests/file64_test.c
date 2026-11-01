/* vm2 studios | t.me/v2vproj — public wide VFS API, real sparse volumes. */
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "fs_backend.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static FILE *image;static u32 sectors,reads;static char output[1024];static u32 output_n;
int disk_read(u32 l,u8*b){reads++;if(l>=sectors||fseeko(image,(off_t)l*512,SEEK_SET))return IO;return fread(b,1,512,image)==512?0:IO;}
int disk_write(u32 l,const u8*b){(void)l;(void)b;assert(!"wide read attempted write");return IO;}
int disk_flush(void){return 0;}u32 disk_sectors(void){return sectors;}int disk_writable(void){return 1;}
void putch(char c){assert(output_n<sizeof(output)-1);output[output_n++]=c;output[output_n]=0;}void out(const char*s){while(*s)putch(*s++);}
static u8 expect(uint64_t at,uint64_t size){
 if(size<UINT32_MAX&&at>=size-16)return "FAT32-NEAR-LIMIT"[(u32)(at-(size-16))];
 const uint64_t pos[]={0,UINT64_C(4294967288),UINT64_C(4294967807),UINT64_C(5368709227)};
 const char*text[]={"WIDE-START","CROSS-4G-BOUNDARY","SECTOR-64","END-OF-WIDE-FILE"};
 for(u32 i=0;i<4;i++)if(pos[i]<size&&at>=pos[i]&&at-pos[i]<strlen(text[i]))return text[i][(u32)(at-pos[i])];return 0;
}
int main(int argc,char**argv){assert(argc==3);image=fopen(argv[1],"rb");assert(image);assert(!fseeko(image,0,SEEK_END));off_t bytes=ftello(image);assert(bytes>0&&bytes%512==0&&(uint64_t)bytes/512<=UINT32_MAX);sectors=bytes/512;
 uint64_t size=!strcmp(argv[2],"fat32")?UINT64_C(4294901760):UINT64_C(5368709243);assert(!fs_mount());MultiFile64 f;memset(&f,0xa5,sizeof(f));assert(!fs_open64("/wide.bin",&f));uint64_t valid=!strcmp(argv[2],"exfat-uninit")?UINT64_C(4294967292):size;assert(f.size==size&&f.valid==valid);
 if(size>UINT32_MAX){u32 before=reads;u8 b;assert(fs_read(f.data,0,&b,1)==UNSUP&&reads==before);}
 const uint64_t offsets[]={0,1,4093,UINT64_C(4294967264),UINT64_C(4294967295),UINT64_C(4294967296),UINT64_C(4294967780),UINT64_C(5368709179),UINT64_MAX-2,UINT64_MAX,size-64,size-1,size};
 const u32 lengths[]={0,1,2,31,64,512,1024};u8 buf[1024];u32 tests=0;
 for(u32 i=0;i<sizeof(offsets)/sizeof(*offsets);i++)for(u32 j=0;j<sizeof(lengths)/sizeof(*lengths);j++){
  uint64_t off=offsets[i];u32 n=lengths[j],before=reads;memset(buf,0xa5,sizeof(buf));int r=fs_read64(&f,off,buf,n);
  if(off>size||(uint64_t)n>size-off){assert(r<0&&reads==before);for(u32 k=0;k<sizeof(buf);k++)assert(buf[k]==0xa5);}
  else{assert(!r);for(u32 k=0;k<n;k++)assert(buf[k]==(off+k<valid?expect(off+k,size):0));for(u32 k=n;k<sizeof(buf);k++)assert(buf[k]==0xa5);}tests++;
 }
 assert(!fs_read64(&f,size,NULL,0));
 if(size<=UINT32_MAX){MultiFile64 bad=f;bad.size=UINT64_C(0x100000000);bad.valid=bad.size;u32 before=reads;assert(fs_read64(&bad,0,buf,1)<0&&reads==before);}assert(fs_read64(&f,size,buf,1)<0);assert(fs_read64(&f,0,NULL,1)==INVALID);
 MultiFile64 old=f;assert(!fs_mount());u32 before=reads;assert(fs_read64(&old,0,buf,1)==BAD&&reads==before);assert(!fs_open64("/wide.bin",&f));
 assert(fs_mount_select(-1)==INVALID);before=reads;assert(fs_read64(&f,0,buf,1)==BAD&&reads==before);
 /* Output is also portable on a freestanding 32-bit target: no libc printf. */
 output_n=0;number64(UINT64_MAX);assert(!strcmp(output,"18446744073709551615"));output_n=0;number64(0);assert(!strcmp(output,"0"));output_n=0;hexnum64(UINT64_C(0x12345678abcdef01));assert(!strcmp(output,"12345678ABCDEF01"));
 fclose(image);printf("PASS %s: %u wide offset/bounds cases; EOF, null, no writes, stale handles, failed mount invalidation, uint64 formatting.\n",argv[2],tests);return 0;}
