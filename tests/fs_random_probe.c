/* vm2 studios | t.me/v2vproj — bounded random IO, resize and recovery. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
#include <unistd.h>
static u32 writes,fail_at;static int crash,failed;
static int writeback(void*x,u32 l,const u8*b){(void)x;if(failed)return IO;
 if(++writes==fail_at){if(crash){fflush(file);_exit(77);}failed=1;return IO;}
 if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flushback(void*x){(void)x;return failed||fflush(file)?IO:0;}
static void mountdisk(void){MultiHal h={.abi=MULTIOS_HAL_ABI,.name="random IO test",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static u8 original[32769],expected[65539],actual[65539];
static void verify(const char*p,const u8*b,u32 n){MultiFile64 f;int er=fs_open64(p,&f);if(er||f.size!=n)fprintf(stderr,"verify %s open=%d size=%llu expected=%u\n",p,er,(unsigned long long)f.size,n);assert(!er&&f.size==n);assert(!fs_read64(&f,0,actual,n)&&!memcmp(actual,b,n));}
static const u8 patch[9]="PATCH123";
static int mutate(int op){return op?fs_resize64("/DATA.BIN",65539):fs_write64("/DATA.BIN",509,patch,sizeof patch);}
int main(int argc,char**argv){assert(argc>=3);file=fopen(argv[1],"r+b");assert(file);fseeko(file,0,SEEK_END);sectors=(u32)(ftello(file)/512);mountdisk();writes=0;
 for(u32 i=0;i<sizeof original;i++)original[i]=(u8)(i*37+11);
 memcpy(expected,original,sizeof original);
 if(!strcmp(argv[2],"seed")){for(u32 o=0;o<sizeof original;){u32 n=sizeof original-o;if(n>8192)n=8192;assert(!fs_put("/DATA.BIN",original+o,n,o?2:0,0));o+=n;}}
 else if(!strcmp(argv[2],"attempt")){assert(argc==6);fail_at=strtoul(argv[3],0,0);crash=atoi(argv[4]);int r=mutate(atoi(argv[5]));if(!fail_at)assert(!r);else assert(r<0&&failed);printf("result=%d writes=%u\n",r,writes);}
 else if(!strcmp(argv[2],"recover")){assert(argc==4);MultiFile64 f;assert(!fs_open64("/DATA.BIN",&f));int op=atoi(argv[3]);if(op&&f.size==65539){verify("/DATA.BIN",expected,65539);}else{assert(f.size==sizeof original);assert(!fs_read64(&f,0,actual,sizeof original));if(!op){memcpy(expected+509,patch,sizeof patch);assert(!memcmp(actual,original,sizeof original)||!memcmp(actual,expected,sizeof original));}else assert(!memcmp(actual,original,sizeof original));}}
 else if(!strcmp(argv[2],"vld")){u8 b[9000]={0};memcpy(b,"VALID-PREFIX!",13);
  assert(!fs_put("/COPY.BIN",0,0,4,"/UNINIT.BIN"));verify("/COPY.BIN",b,8193);
  assert(!fs_put("/UNINIT.BIN",(u8*)"TAIL",4,2,0));memcpy(b+8193,"TAIL",4);verify("/UNINIT.BIN",b,8197);
  assert(!fs_write64("/UNINIT2.BIN",2,(u8*)"XX",2));memset(b,0,sizeof b);memcpy(b,"VAXXD-PREFIX!",13);verify("/UNINIT2.BIN",b,8193);
  assert(!fs_resize64("/UNINIT3.BIN",4099));memset(b,0,sizeof b);memcpy(b,"VALID-PREFIX!",13);verify("/UNINIT3.BIN",b,4099);
  int r=fs_put("/UNINIT4.BIN",(u8*)"TAIL",4,2,0);assert(r==NOSPACE);mountdisk();MultiFile64 f;assert(!fs_open64("/UNINIT4.BIN",&f)&&f.size==3*1024*1024&&f.valid==13);
  puts("PASS exFAT ValidDataLength: atomic copy zero-masking, append/write-at/shrink materialization, oversized materialization rollback");}
 else if(!strcmp(argv[2],"compressed")){const char*p="/COMPRESSED/RANDOM.BIN";
  assert(!fs_put(p,original,sizeof original,0,0));assert(!fs_write64(p,509,patch,sizeof patch));memcpy(expected+509,patch,sizeof patch);verify(p,expected,sizeof original);
  assert(!fs_resize64(p,65539));verify(p,expected,65539);assert(!fs_resize64(p,128));assert(!fs_resize64(p,65539));memset(expected+128,0,sizeof expected-128);verify(p,expected,65539);assert(!fs_remove(p,0));
  puts("PASS NTFS LZNT1 compressed random write/grow/shrink/regrow and delete");}
 else if(!strcmp(argv[2],"wide")){MultiFile64 f;const uint64_t size=UINT64_C(5)*1024*1024*1024+17;
  assert(!fs_open64("/WIDE.BIN",&f)&&f.size==size);u8 b[32];assert(!fs_read64(&f,size-32,b,32));for(u32 i=0;i<32;i++)assert(!b[i]);
  assert(!fs_write64("/WIDE.BIN",size-9,patch,9));assert(fs_read64(&f,size-9,b,9)==BAD);
  assert(!fs_open64("/WIDE.BIN",&f)&&!fs_read64(&f,size-9,b,9)&&!memcmp(b,patch,9));
  assert(!fs_resize64("/WIDE.BIN",size+19));assert(!fs_open64("/WIDE.BIN",&f)&&f.size==size+19);assert(!fs_read64(&f,size,b,19));for(u32 i=0;i<19;i++)assert(!b[i]);
  assert(!fs_resize64("/WIDE.BIN",size-9));assert(!fs_open64("/WIDE.BIN",&f)&&f.size==size-9);
  puts("PASS real >5 GiB file: write above 4 GiB, stale view, grow zero-fill and shrink; no 32-bit truncation");}
 else if(!strcmp(argv[2],"suite")){MultiFile64 f;assert(!fs_open64("/DATA.BIN",&f));u32 before=writes;
  assert(!fs_write64("/DATA.BIN",f.size,0,0)&&!fs_resize64("/DATA.BIN",f.size)&&writes==before);assert(!fs_read64(&f,0,actual,1));
  assert(fs_write64("/DATA.BIN",UINT64_MAX,patch,1)==INVALID&&writes==before);
  assert(fs_write64("/DATA.BIN",f.size,patch,1)==INVALID&&writes==before);
  assert(fs_write64("/DATA.BIN",0,0,1)==INVALID&&writes==before);
  assert(fs_resize64("/DATA.BIN",UINT64_MAX)==UNSUP&&writes==before);
  if(vol.kind==FS_FAT32)assert(fs_resize64("/DATA.BIN",UINT64_C(0x100000000))==UNSUP&&writes==before);
  vol.writable=0;assert(fs_write64("/DATA.BIN",0,patch,1)==READONLY&&fs_resize64("/DATA.BIN",2)==READONLY&&writes==before);assert(!fs_unlock());
  assert(!mutate(0));assert(fs_read64(&f,0,actual,1)==BAD);memcpy(expected+509,patch,sizeof patch);verify("/DATA.BIN",expected,sizeof original);
  assert(!mutate(1));verify("/DATA.BIN",expected,65539);
  assert(!fs_resize64("/DATA.BIN",513));verify("/DATA.BIN",expected,513);
  assert(!fs_resize64("/DATA.BIN",0));verify("/DATA.BIN",expected,0);
  assert(!fs_resize64("/DATA.BIN",65539));memset(expected,0,sizeof expected);verify("/DATA.BIN",expected,65539);
  assert(fs_write64("/ABSENT",0,patch,1)==NOTFOUND);assert(fs_resize64("/ABSENT",1)==NOTFOUND);
  puts("PASS random IO cross-sector, resize growth zero-fill/shrink/zero/regrow, stale views, no-op, range/overflow/RO/FAT32 limits");}
 else assert(0);
 if(!failed)assert(!disk_flush());fclose(file);return 0;}
