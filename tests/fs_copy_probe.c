/* vm2 studios | t.me/v2vproj — staged copy tests on real filesystem/HAL. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include "multios/copy.h"
#include <string.h>
#include <unistd.h>
static u32 writes, fail_at; static int crash, failed, read_fail;
static int readcopy(void*x,u32 l,u8*b){if(read_fail){read_fail=0;return IO;}return rd(x,l,b);}
static int writecopy(void*x,u32 l,const u8*b){(void)x;
 if(failed)return IO;
 if(++writes==fail_at){if(crash){fflush(file);_exit(77);}failed=1;return IO;}
 if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;
 return fwrite(b,1,512,file)==512?0:IO;
}
static int flushcopy(void*x){(void)x;return failed||fflush(file)?IO:0;}
static void mountcopy(void){
 failed=0;fail_at=0;read_fail=0;
 /* Simulate fresh process: discard VFS state before rebinding the HAL. */
 vol.mounted=0;vol.writable=0;
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="staged copy probe",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=readcopy,.write=writecopy,.flush=flushcopy},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());
}
static void check(const char*p,const char*source){MultiFile64 a,b;assert(!fs_open64(p,&a)&&!fs_open64(source,&b)&&a.size==b.size);u8 x[8192],y[8192];
 for(uint64_t o=0;o<a.size;){u32 n=a.size-o>sizeof x?sizeof x:(u32)(a.size-o);assert(!fs_read64(&a,o,x,n)&&!fs_read64(&b,o,y,n)&&!memcmp(x,y,n));o+=n;}}
static int cancel(void*x,uint64_t off,uint64_t total,int phase){(void)x;(void)total;return phase==MULTIOS_COPY_DATA&&off>=3*MULTIOS_COPY_CHUNK;}
static int cancel_before(void*x,uint64_t off,uint64_t total,int phase){(void)x;(void)off;(void)total;(void)phase;return 1;}
static int fail_verify(void*x,uint64_t off,uint64_t total,int phase){(void)x;(void)off;(void)total;if(phase==MULTIOS_COPY_VERIFY)read_fail=1;return 0;}
static void recover_test(void){
 MultiFile64 f;int r=fs_open64("/RECOVER.BIN",&f);
 if(!r){check("/RECOVER.BIN","/SMALL.BIN");assert(fs_open64("/MC000001.TMP",&f)==NOTFOUND);}
 else {assert(r==NOTFOUND);r=fs_open64("/MC000001.TMP",&f);
  if(!r){MultiFile64 a;assert(!fs_open64("/SMALL.BIN",&a)&&f.size<=a.size);u8 x[512],y[512];for(uint64_t o=0;o<f.size;){u32 n=f.size-o>512?512:(u32)(f.size-o);assert(!fs_read64(&f,o,x,n)&&!fs_read64(&a,o,y,n)&&!memcmp(x,y,n));o+=n;}
   MultiCopyResult c;assert(!fs_copy_staged("/SMALL.BIN","/RECOVER.BIN","/MC000001.TMP",&c,0,0));
  }else{assert(r==NOTFOUND);MultiCopyResult c;assert(!fs_copy_staged("/SMALL.BIN","/RECOVER.BIN",0,&c,0,0));}
  check("/RECOVER.BIN","/SMALL.BIN");
 }
 puts("PASS recovery: final complete or matching committed staging prefix; explicit resume complete");
}
int main(int argc,char**argv){assert(argc>=3);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountcopy();writes=0;
 if(!strcmp(argv[2],"seed")){assert(argc==4);FILE*in=fopen(argv[3],"rb");assert(in);u8 buffer[8192];u32 n;int first=1;while((n=fread(buffer,1,sizeof buffer,in))){assert(!fs_put("/SOURCE.BIN",buffer,n,first?0:2,0));first=0;}fclose(in);
  assert(!fs_open64("/SOURCE.BIN",&(MultiFile64){0}));in=fopen(argv[3],"rb");assert(in);u32 remaining=32769;first=1;while(remaining){n=remaining>sizeof buffer?sizeof buffer:remaining;assert(fread(buffer,1,n,in)==n);assert(!fs_put("/SMALL.BIN",buffer,n,first?0:2,0));first=0;remaining-=n;}fclose(in);assert(!disk_flush());fclose(file);puts("PASS seeded large and small fixtures through VFS");return 0;}
 if(!strcmp(argv[2],"attempt")){assert(argc==5);fail_at=(u32)strtoul(argv[3],0,0);crash=atoi(argv[4]);MultiCopyResult c;int r=fs_copy_staged("/SMALL.BIN","/RECOVER.BIN",0,&c,0,0);if(!fail_at)assert(!r&&c.published);else assert(r<0&&failed);printf("result=%d writes=%u stage=%s\n",r,writes,c.staging);fclose(file);return 0;}
 if(!strcmp(argv[2],"recover")){recover_test();fclose(file);return 0;}
 assert(!strcmp(argv[2],"suite"));MultiCopyResult c;MultiFile64 f;u32 before;
 assert(fs_copy_staged("/SOURCE.BIN","/LARGE.BIN",0,&c,cancel,0)==CANCELLED&&!c.published&&c.completed==3*MULTIOS_COPY_CHUNK);
 assert(fs_open64("/LARGE.BIN",&f)==NOTFOUND);char stage[PATHMAX];strcpy(stage,c.staging);mountcopy();
 assert(!fs_copy_staged("/SOURCE.BIN","/LARGE.BIN",stage,&c,0,0)&&c.published&&c.total>4*1024*1024);check("/LARGE.BIN","/SOURCE.BIN");assert(fs_open64(stage,&f)==NOTFOUND);
 before=writes;assert(fs_copy_staged("/SOURCE.BIN","/LARGE.BIN",0,&c,0,0)==EXISTS&&writes==before);
 assert(!fs_put("/WRONG.TMP",(const u8*)"wrong",5,0,0));before=writes;assert(fs_copy_staged("/SOURCE.BIN","/WRONGDEST.BIN","/WRONG.TMP",&c,0,0)==BAD&&writes==before);
 assert(!fs_open64("/WRONG.TMP",&f)&&f.size==5);u8 wrong[5];assert(!fs_read64(&f,0,wrong,5)&&!memcmp(wrong,"wrong",5));
 before=writes;assert(fs_copy_staged("/SMALL.BIN","/CANCEL.BIN",0,&c,cancel_before,0)==CANCELLED&&!c.staging[0]&&writes==before);
 assert(fs_copy_staged("/SMALL.BIN","/a/../BAD",0,&c,0,0)==INVALID&&writes==before);
 assert(fs_copy_staged("/SMALL.BIN","/trailing.",0,&c,0,0)==INVALID&&writes==before);
 assert(!fs_put("/EMPTY.BIN",0,0,0,0));
 before=writes;assert(fs_copy_staged("/EMPTY.BIN","/STOLEN.BIN","/empty.bin",&c,0,0)==INVALID&&writes==before);
 assert(!fs_copy_staged("/EMPTY.BIN","/EMPTYCOPY.BIN",0,&c,0,0));check("/EMPTYCOPY.BIN","/EMPTY.BIN");
 assert(!fs_put("/Long Empty Filename.txt",0,0,0,0));Stream ep;Entry ee;int er;assert(!fs_resolve("/Long Empty Filename.txt",&ep,&ee,&er));
 if(ee.alias[0]){char alias[PATHMAX]="/";strcat(alias,ee.alias);before=writes;assert(fs_copy_staged("/Long Empty Filename.txt","/STOLEN2.BIN",alias,&c,0,0)==INVALID&&writes==before);}
 /* Predictable-looking destination must not be selected as its own staging. */
 assert(!fs_copy_staged("/SMALL.BIN","/mc000003.tmp",0,&c,0,0));check("/mc000003.tmp","/SMALL.BIN");
 assert(!fs_put("/NESTED",0,0,3,0));assert(!fs_copy_staged("/SMALL.BIN","/NESTED/копия.bin",0,&c,0,0));check("/NESTED/копия.bin","/SMALL.BIN");
 int vr=fs_copy_staged("/SMALL.BIN","/VERIFY.BIN",0,&c,fail_verify,0);assert(vr<0&&!c.published);strcpy(stage,c.staging);mountcopy();assert(fs_open64("/VERIFY.BIN",&f)==NOTFOUND);assert(!fs_copy_staged("/SMALL.BIN","/VERIFY.BIN",stage,&c,0,0));check("/VERIFY.BIN","/SMALL.BIN");
 before=writes;vol.writable=0;assert(fs_copy_staged("/SMALL.BIN","/RO.BIN",0,&c,0,0)==READONLY&&writes==before);assert(!fs_unlock());
 assert(!disk_flush());fclose(file);puts("PASS >4 MiB staged copy; cancel/remount/resume, prefix mismatch nonmutation, no overwrite, empty/Unicode/nested paths, staging alias guard, final read error, RO guard");return 0;
}
