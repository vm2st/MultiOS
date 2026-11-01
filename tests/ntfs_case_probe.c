/* MultiOS / MultiPulti — real case-colliding NTFS namespace probe. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static u32 writes;
static int writeback(void*x,u32 l,const u8*b){(void)x;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;writes++;return fwrite(b,1,512,file)==512?0:IO;}
static int flushback(void*x){(void)x;return fflush(file)?IO:0;}
static void mountdisk(void){MultiHal h={.abi=MULTIOS_HAL_ABI,.name="NTFS exact namespace",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static void repeated(const char*path,u8 value,u32 n){MultiFile64 f;u8 b[8197];int result=fs_open64(path,&f);if(result||f.size!=n)fprintf(stderr,"open %s result=%d size=%llu expected=%u\n",path,result,result?0:(unsigned long long)f.size,n);assert(!result&&f.size==n);assert(!fs_read64(&f,0,b,n));for(u32 i=0;i<n;i++)assert(b[i]==value);}
static void stream(const char*name,const char*want){u8 b[32];u32 n=(u32)strlen(want);assert(!fs_stream_read("/Stream.bin",name,0,b,n)&&!memcmp(b,want,n));}
int main(int argc,char**argv){assert(argc==3);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();
 if(!strcmp(argv[2],"inspect")){
  const char*paths[]={"/File.txt","/file.txt","/FILE.TXT","/CaseDir/item.bin","/casedir/item.bin"};
  for(u32 i=0;i<5;i++){MultiFile64 f;int r=fs_open64(paths[i],&f);u8 b=0;if(!r)r=fs_read64(&f,0,&b,1);printf("%s result=%d size=%llu first=%c\n",paths[i],r,r?0:(unsigned long long)f.size,b);}
  const char*names[]={"Notes","notes","NOTES"};for(u32 i=0;i<3;i++){u8 b[5]={0};int r=fs_stream_read("/Stream.bin",names[i],0,b,5);printf("ADS %s result=%d bytes=%.*s\n",names[i],r,5,b);}
 }else if(!strcmp(argv[2],"read")){
  u32 before=writes;repeated("/File.txt",'A',8193);repeated("/file.txt",'b',8197);repeated("/CaseDir/item.bin",'U',4097);repeated("/casedir/item.bin",'d',8193);
  MultiFile64 f;assert(fs_open64("/FILE.TXT",&f)==UNSUP);assert(fs_open64("/CASEDIR/item.bin",&f)==UNSUP);
  assert(!fs_open64("/unique.txt",&f)&&f.size==6);stream("Notes","FIRST");stream("notes","second-long");stream("ONLY","UNIQUE");u8 b[5]={1,2,3,4,5};assert(fs_stream_read("/Stream.bin","NOTES",0,b,5)==UNSUP&&b[0]==1&&b[4]==5);assert(writes==before);puts("PASS exact file/directory/ADS selection; unique folded fallback; ambiguous folded reads rejected without writes/output");
 }else if(!strcmp(argv[2],"mutate")){
  assert(!fs_put("/file.txt",(const u8*)"LOWER",5,1,0));repeated("/File.txt",'A',8193);
  assert(!fs_write64("/CaseDir/item.bin",509,(const u8*)"UPPER",5));repeated("/casedir/item.bin",'d',8193);
  assert(!fs_stream_change("/Stream.bin","notes",MULTIOS_STREAM_REPLACE,0,(const u8*)"LOWER-ADS",9));stream("Notes","FIRST");stream("notes","LOWER-ADS");
  assert(!fs_put("/ExactCopy.bin",0,0,4,"/File.txt"));repeated("/ExactCopy.bin",'A',8193);
  assert(!fs_link("/File.txt","/Alias.bin"));repeated("/Alias.bin",'A',8193);
  assert(!fs_rename("/File.txt","Renamed.txt"));assert(!fs_remove("/Renamed.txt",0));repeated("/Alias.bin",'A',8193);
  puts("PASS exact replace/write-at/ADS replace/copy/hardlink/rename/delete preserve case-distinct siblings");
 }else if(!strcmp(argv[2],"guards")){
  u32 before=writes;
  assert(fs_put("/FILE.TXT",(const u8*)"BAD",3,1,0)==UNSUP);
  assert(fs_put("/FILE.TXT",0,0,3,0)==UNSUP);
  assert(fs_put("/Copy.bin",0,0,4,"/FILE.TXT")==UNSUP);
  assert(fs_put("/CASEDIR/new.bin",(const u8*)"BAD",3,0,0)==UNSUP);
  assert(fs_write64("/FILE.TXT",0,(const u8*)"BAD",3)==UNSUP);
  assert(fs_resize64("/FILE.TXT",0)==UNSUP);
  assert(fs_remove("/FILE.TXT",0)==UNSUP);
  assert(fs_remove("/CASEDIR",1)==UNSUP);
  assert(fs_rename("/FILE.TXT","Other.txt")==UNSUP);
  assert(fs_rename("/Unique.TXT","FILE.TXT")==UNSUP);
  assert(fs_rename("/File.txt","file.txt")==EXISTS);
  assert(fs_link("/FILE.TXT","Alias.bin")==UNSUP);
  assert(fs_link("/Unique.TXT","/FILE.TXT")==UNSUP);
  assert(fs_stream_change("/FILE.TXT","test",MULTIOS_STREAM_CREATE,0,(const u8*)"BAD",3)==UNSUP);
  assert(writes==before);
  for(int op=MULTIOS_STREAM_CREATE;op<=MULTIOS_STREAM_REMOVE;op++){
   assert(!fs_mount()&&!fs_unlock());assert(fs_stream_change("/Stream.bin","NOTES",op,0,0,0)==UNSUP);
  }
  assert(!fs_mount()&&!fs_unlock());repeated("/File.txt",'A',8193);repeated("/file.txt",'b',8197);stream("Notes","FIRST");stream("notes","second-long");
  puts("PASS ambiguous file/parent/source/destination mutations rejected before writes; ambiguous ADS changes rejected; exact siblings unchanged");
 }else assert(0);
 assert(!disk_flush());fclose(file);return 0;}
