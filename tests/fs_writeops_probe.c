/* MultiOS / MultiPulti — real HAL/VFS write semantics regression. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static u32 writes;
static u8 source_data[8193];
static int writeback(void*x,u32 l,const u8*b){(void)x;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;writes++;return fwrite(b,1,512,file)==512?0:IO;}
static int flushback(void*x){(void)x;return fflush(file)?IO:0;}
static void mountdisk(void){if(vol.mounted){assert(!fs_mount()&&!fs_unlock());return;}MultiHal h={.abi=MULTIOS_HAL_ABI,.name="write semantics",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static void check(const char*p,const char*expected){MultiFile64 f;u8 b[64];assert(!fs_open64(p,&f)&&f.size==strlen(expected));assert(!fs_read64(&f,0,b,(u32)f.size)&&!memcmp(b,expected,(size_t)f.size));}
static void verify_source(const char*p){MultiFile64 f;u8 data[sizeof source_data];assert(!fs_open64(p,&f)&&f.size==sizeof source_data);assert(!fs_read64(&f,0,data,sizeof data)&&!memcmp(data,source_data,sizeof data));}
static void exact_name(Stream dir,const char*name){u32 cursor=0;Entry e;int r,found=0;while((r=fs_next(dir,&cursor,&e))>0)if(!strcmp(e.name,name))found++;assert(!r&&found==1);}
static void protected_source(void){Stream parent;Entry e;int isroot;assert(!fs_resolve("/ReadOnly.txt",&parent,&e,&isroot)&&!isroot&&(e.attr&5)==5);verify_source("/ReadOnly.txt");}
static void rejected(int r,int expected){assert(r==expected);mountdisk();protected_source();}
int main(int argc,char**argv){assert(argc==3);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();
 for(u32 i=0;i<sizeof source_data;i++)source_data[i]=(u8)(i*37+11);
 int result;
 if(!strcmp(argv[2],"seed")){
  assert(!fs_put("/ReadOnly.txt",source_data,sizeof source_data,0,0));
  assert(!fs_put("/old.txt",(const u8*)"original",8,0,0));assert(!fs_put("/empty",0,0,3,0));
  assert(!fs_put("/MixedCase.txt",(const u8*)"case file",9,0,0));
  assert(!fs_put("/MixedDir",0,0,3,0));assert(!fs_put("/MixedDir/child.txt",(const u8*)"nested",6,0,0));result=0;
 }else if(!strcmp(argv[2],"case")){
  result=fs_rename("/MixedCase.txt","mixedcase.TXT");printf("file case-only rename=%d\n",result);if(!result){check("/mixedcase.TXT","case file");result=fs_rename("/MixedDir","mixeddir");printf("directory case-only rename=%d\n",result);if(!result){check("/mixeddir/child.txt","nested");exact_name(fs_root(),"mixedcase.TXT");exact_name(fs_root(),"mixeddir");}
   if(!result){assert(!fs_put("/Collision.txt",(const u8*)"collision",9,0,0));rejected(fs_rename("/mixedcase.TXT","Collision.txt"),EXISTS);check("/mixedcase.TXT","case file");check("/Collision.txt","collision");}}
 }else if(!strcmp(argv[2],"copy-ro")){
  result=fs_put("/CopiedReadOnly.txt",0,0,4,"/ReadOnly.txt");printf("copy readonly source=%d\n",result);if(!result){protected_source();verify_source("/CopiedReadOnly.txt");
   assert(!fs_write64("/CopiedReadOnly.txt",509,(const u8*)"LOCAL",5));MultiFile64 f;u8 bytes[5];assert(!fs_open64("/CopiedReadOnly.txt",&f)&&!fs_read64(&f,509,bytes,5)&&!memcmp(bytes,"LOCAL",5));protected_source();}
 }else if(!strcmp(argv[2],"guards")){
  rejected(fs_put("/ReadOnly.txt",(const u8*)"bad",3,1,0),READONLY);
  rejected(fs_put("/ReadOnly.txt",(const u8*)"bad",3,2,0),READONLY);
  rejected(fs_write64("/ReadOnly.txt",0,(const u8*)"bad",3),READONLY);
  rejected(fs_resize64("/ReadOnly.txt",3),READONLY);
  rejected(fs_remove("/ReadOnly.txt",0),READONLY);
  rejected(fs_rename("/ReadOnly.txt","Changed.txt"),READONLY);
  result=fs_put("/ReadOnly.txt",0,0,4,"/MixedCase.txt");assert(result==READONLY||result==EXISTS);mountdisk();protected_source();
  rejected(fs_remove("/MixedDir",1),NOTEMPTY);
  result=fs_put("/DirectoryCopy",0,0,4,"/MixedDir");assert(result==ISDIR);mountdisk();result=0;
  puts("PASS readonly/system source preserved; replace/append/write-at/resize/delete/rename denied; protected target and nonempty directory protected");
 }else assert(0);
 if(!result)assert(!disk_flush());fclose(file);return result?1:0;
}
