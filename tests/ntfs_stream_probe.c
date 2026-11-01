/* vm2 studios | t.me/v2vproj — bounded random IO, resize and recovery. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
#include <unistd.h>
#include "multios/copy.h"
static u32 writes,fail_at;static int crash,failed;
static int writeback(void*x,u32 l,const u8*b){(void)x;if(failed)return IO;
 if(++writes==fail_at){if(crash){fflush(file);_exit(77);}failed=1;return IO;}
 if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flushback(void*x){(void)x;return failed||fflush(file)?IO:0;}
static void mountdisk(void){vol.mounted=0;vol.writable=0;failed=0;fail_at=0;MultiHal h={.abi=MULTIOS_HAL_ABI,.name="random IO test",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static u8 original[32769],expected[65539],actual[65539];
static void verify(const char*p,const u8*b,u32 n){MultiFile64 f;int er=fs_open64(p,&f);if(er||f.size!=n)fprintf(stderr,"verify %s open=%d size=%llu expected=%u\n",p,er,(unsigned long long)f.size,n);assert(!er&&f.size==n);assert(!fs_read64(&f,0,actual,n)&&!memcmp(actual,b,n));}
static const u8 patch[9]="PATCH123";
static int mutate(int op){return op?fs_resize64("/DATA.BIN",65539):fs_write64("/DATA.BIN",509,patch,sizeof patch);}
static const char *file_path="/FILE.BIN";
static void streamcheck(const char*path,const char*name,const u8*b,u32 n){
 u32 cursor=0;MultiNamedStream info;int r,found=0;
 while((r=fs_stream_next(path,&cursor,&info))>0){if(!strcmp(info.name,name)){assert(info.size==n);found++;}}
 assert(!r&&found==1);assert(!fs_stream_read(path,name,0,actual,n)&&!memcmp(actual,b,n));
}
static int cancel_ads(void*x,uint64_t off,uint64_t total,int phase){(void)x;(void)total;return phase==MULTIOS_COPY_STREAM_DATA&&off>=8192;}
static int operation(int op){switch(op){
 case 0:return fs_stream_change(file_path,"NEW",MULTIOS_STREAM_CREATE,0,original,8193);
 case 1:return fs_stream_change(file_path,"DATA",MULTIOS_STREAM_WRITE,509,patch,9);
 case 2:return fs_stream_change(file_path,"DATA",MULTIOS_STREAM_RESIZE,65539,0,0);
 case 3:return fs_stream_change(file_path,"DATA",MULTIOS_STREAM_REMOVE,0,0,0);
 case 4:return fs_link(file_path,"/DOCS/FAULT.BIN");
 default:{MultiCopyResult c;return fs_copy_streams(file_path,"/COPYADS.BIN",0,&c,0,0);}}}
int main(int argc,char**argv){assert(argc>=3);file=fopen(argv[1],"r+b");assert(file);fseeko(file,0,SEEK_END);sectors=ftello(file)/512;if(!strcmp(argv[2],"deny")){
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="deny test",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};
 writes=0;assert(!multi_hal_bind(&h));int m=fs_mount(),u=m<0?m:fs_unlock();assert(u<0&&!vol.writable&&!writes);
 assert(fs_stream_change("/resident.txt","no",MULTIOS_STREAM_CREATE,0,(u8*)"x",1)==READONLY&&fs_link("/resident.txt","/no")==READONLY&&!writes);
 fclose(file);puts("PASS unsafe NTFS denied RW without any raw write");return 0;
 }mountdisk();writes=0;
 for(u32 i=0;i<sizeof original;i++)original[i]=(u8)(i*37+11);memcpy(expected,original,sizeof original);
 if(!strcmp(argv[2],"seed")){
  for(u32 o=0;o<sizeof original;){u32 n=sizeof original-o;if(n>8192)n=8192;assert(!fs_put(file_path,original+o,n,o?2:0,0));o+=n;}
  assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_CREATE,0,original,8193));
  assert(!fs_stream_change(file_path,"Keep",MULTIOS_STREAM_CREATE,0,(u8*)"PRESERVE",8));
 }else if(!strcmp(argv[2],"attempt")){
  assert(argc==6);fail_at=strtoul(argv[3],0,0);crash=atoi(argv[4]);int r=operation(atoi(argv[5]));
  if(!fail_at)assert(!r);else assert(r<0&&failed);printf("result=%d writes=%u\n",r,writes);
 }else if(!strcmp(argv[2],"recover")){
  assert(argc==4);int op=atoi(argv[3]);verify(file_path,original,sizeof original);streamcheck(file_path,"Keep",(u8*)"PRESERVE",8);
  if(op==5){MultiFile64 f;int r=fs_open64("/COPYADS.BIN",&f);if(r==NOTFOUND){
    MultiCopyResult c;int s=fs_open64("/MC000001.TMP",&f);assert(s==0||s==NOTFOUND);
    assert(!fs_copy_streams(file_path,"/COPYADS.BIN",s==0?"/MC000001.TMP":0,&c,0,0));
   }else assert(!r);verify("/COPYADS.BIN",original,sizeof original);streamcheck("/COPYADS.BIN","DATA",original,8193);streamcheck("/COPYADS.BIN","Keep",(u8*)"PRESERVE",8);
  }else if(op==4){MultiFile64 a,b;assert(!fs_open64(file_path,&a));int r=fs_open64("/DOCS/FAULT.BIN",&b);assert(r==NOTFOUND||(!r&&b.data.first==a.data.first));}
  else{
   if(op==2)memset(expected+8193,0,sizeof expected-8193);
   const char*name=op==0?"NEW":"DATA";u32 c=0;MultiNamedStream info;int r,found=0;
   while((r=fs_stream_next(file_path,&c,&info))>0)if(!strcmp(info.name,name)){found=1;
    if(op==0)assert(info.size==8193);else if(op==2)assert(info.size==8193||info.size==65539);else assert(info.size==8193);
    assert(!fs_stream_read(file_path,name,0,actual,(u32)info.size));
    if(op==1){memcpy(expected+509,patch,9);assert(!memcmp(actual,original,8193)||!memcmp(actual,expected,8193));}
    else assert(!memcmp(actual,expected,(u32)info.size));
   }
   assert(!r);if(op==1||op==2)assert(found);
  }
 }else if(!strcmp(argv[2],"compressed")){
  const char*p="/COMPRESSED/lznt1.bin";assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_CREATE,0,original,sizeof original));streamcheck(p,"CompressedADS",original,sizeof original);
  assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_WRITE,509,patch,9));memcpy(expected+509,patch,9);streamcheck(p,"CompressedADS",expected,sizeof original);
  assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_RESIZE,65539,0,0));streamcheck(p,"CompressedADS",expected,65539);
  assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_RESIZE,13,0,0));assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_RESIZE,65539,0,0));memset(expected+13,0,sizeof expected-13);streamcheck(p,"CompressedADS",expected,65539);
  u32 cur=0;MultiNamedStream inf;int found=0;while(fs_stream_next(p,&cur,&inf)>0)if(!strcmp(inf.name,"CompressedADS")){assert(inf.flags&1);found=1;}assert(found);
  assert(!fs_stream_change(p,"CompressedADS",MULTIOS_STREAM_REMOVE,0,0,0));puts("PASS NTFS compressed ADS create/random-write/exact-grow/shrink/regrow/delete");
 }else if(!strcmp(argv[2],"suite")){
  u32 before=writes;u8 ext[3];assert(!fs_stream_read("/resident.txt","external",0,ext,3)&&!memcmp(ext,"EXT",3));assert(writes==before);
  assert(fs_stream_read(file_path,"",0,ext,1)==INVALID&&writes==before);
  assert(fs_stream_change(file_path,"bad:name",0,0,ext,1)==INVALID&&writes==before);
  assert(fs_stream_change(file_path,"bad",99,0,ext,1)==INVALID&&writes==before);
  assert(fs_stream_change(file_path,"bad",MULTIOS_STREAM_RESIZE,UINT64_MAX,0,0)==INVALID&&writes==before);
  vol.writable=0;assert(fs_stream_change(file_path,"no",0,0,ext,1)==READONLY&&fs_link(file_path,"/NO")==READONLY&&writes==before);assert(!fs_unlock());
  MultiFile64 view;assert(!fs_open64(file_path,&view));
  assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_REPLACE,0,original,sizeof original));assert(fs_read64(&view,0,ext,1)==BAD);streamcheck(file_path,"DATA",original,sizeof original);
  assert(!fs_stream_change(file_path,"data",MULTIOS_STREAM_APPEND,0,(u8*)"TAIL",4));memcpy(expected+sizeof original,"TAIL",4);streamcheck(file_path,"DATA",expected,sizeof original+4);
  assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_WRITE,509,patch,9));memcpy(expected+509,patch,9);streamcheck(file_path,"DATA",expected,sizeof original+4);
  assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_RESIZE,65539,0,0));streamcheck(file_path,"DATA",expected,65539);
  assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_RESIZE,13,0,0));assert(!fs_stream_change(file_path,"DATA",MULTIOS_STREAM_RESIZE,8193,0,0));memset(expected+13,0,sizeof expected-13);streamcheck(file_path,"DATA",expected,8193);
  const uint64_t huge=UINT64_C(5)*1024*1024*1024+17;
  assert(!fs_stream_change(file_path,"BIG",MULTIOS_STREAM_CREATE,0,0,0));
  assert(!fs_stream_change(file_path,"BIG",MULTIOS_STREAM_RESIZE,huge,0,0));
  assert(!fs_stream_change(file_path,"BIG",MULTIOS_STREAM_WRITE,huge-9,patch,9));
  assert(!fs_stream_read(file_path,"BIG",huge-9,ext,3)&&!memcmp(ext,patch,3));
  assert(!fs_stream_read(file_path,"BIG",UINT64_C(1)<<32,ext,3)&&!ext[0]&&!ext[1]&&!ext[2]);
  assert(!fs_stream_change(file_path,"BIG",MULTIOS_STREAM_RESIZE,8193,0,0));
  assert(!fs_stream_change(file_path,"BIG",MULTIOS_STREAM_REMOVE,0,0,0));
  assert(!fs_stream_change(file_path,"EMPTY",MULTIOS_STREAM_CREATE,0,0,0));streamcheck(file_path,"EMPTY",expected,0);
  assert(!fs_stream_change(file_path,"EMPTY",MULTIOS_STREAM_REMOVE,0,0,0));assert(fs_stream_read(file_path,"EMPTY",0,ext,1)==NOTFOUND);
  char unicode[321];for(u32 i=0;i<160;i++){unicode[i*2]=(char)0xd0;unicode[i*2+1]=(char)0x96;}unicode[320]=0;
  assert(!fs_stream_change(file_path,unicode,MULTIOS_STREAM_CREATE,0,(u8*)"UNICODE",7));streamcheck(file_path,unicode,(u8*)"UNICODE",7);
  assert(!fs_stream_change(file_path,"Zone.Identifier",MULTIOS_STREAM_CREATE,0,(u8*)"[ZoneTransfer]\r\nZoneId=3\r\n",26));
  MultiCopyResult copy;
  assert(fs_copy_streams(file_path,"/COPIED.BIN",0,&copy,cancel_ads,0)==CANCELLED&&!copy.published);char stage[PATHMAX];strcpy(stage,copy.staging);
  assert(!fs_copy_streams(file_path,"/COPIED.BIN",stage,&copy,0,0)&&copy.published);verify("/COPIED.BIN",original,sizeof original);streamcheck("/COPIED.BIN","DATA",expected,8193);streamcheck("/COPIED.BIN",unicode,(u8*)"UNICODE",7);
  assert(!fs_copy_staged(file_path,"/WRONG.TMP",0,&copy,0,0));assert(!fs_stream_change("/WRONG.TMP","EXTRA",MULTIOS_STREAM_CREATE,0,(u8*)"NO",2));
  before=writes;assert(fs_copy_streams(file_path,"/REFUSED.BIN","/WRONG.TMP",&copy,0,0)==BAD&&writes==before);
  assert(!fs_stream_change("/WRONG.TMP","EXTRA",MULTIOS_STREAM_REMOVE,0,0,0));assert(!fs_stream_change("/WRONG.TMP","DATA",MULTIOS_STREAM_CREATE,0,(u8*)"BAD",3));
  before=writes;assert(fs_copy_streams(file_path,"/REFUSED.BIN","/WRONG.TMP",&copy,0,0)==BAD&&writes==before);
  assert(!fs_remove("/WRONG.TMP",0));
  assert(!fs_link(file_path,"/DOCS/LINK.BIN"));MultiFile64 a,b;assert(!fs_open64(file_path,&a)&&!fs_open64("/DOCS/LINK.BIN",&b)&&a.data.first==b.data.first);
  assert(!fs_write64("/DOCS/LINK.BIN",0,(u8*)"SHARED",6));memcpy(original,"SHARED",6);verify(file_path,original,sizeof original);
  assert(!fs_stream_change("/DOCS/LINK.BIN","Keep",MULTIOS_STREAM_REPLACE,0,(u8*)"LINK-ADS",8));streamcheck(file_path,"Keep",(u8*)"LINK-ADS",8);
  assert(!fs_remove(file_path,0));verify("/DOCS/LINK.BIN",original,sizeof original);streamcheck("/DOCS/LINK.BIN","DATA",expected,8193);
  assert(!fs_rename("/DOCS/LINK.BIN","RENAMED.BIN"));assert(!fs_link("/DOCS/RENAMED.BIN","/FINAL.BIN"));streamcheck("/FINAL.BIN",unicode,(u8*)"UNICODE",7);streamcheck("/FINAL.BIN","Keep",(u8*)"LINK-ADS",8);
  streamcheck("/resident.txt","External",(u8*)"EXT",3);
  puts("PASS ADS resident/nonresident/Unicode/casefold/create/replace/append/write/resize/delete; hardlink cross-directory/shared DATA+ADS/unlink/rename; RO/invalid/stale-view guards; COPYADS cancel/resume and unknown/mismatching staging-stream refusal");
 }else assert(0);
 if(!failed)assert(!disk_flush());fclose(file);return 0;}
