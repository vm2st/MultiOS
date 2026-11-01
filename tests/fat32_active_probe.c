/* MultiOS: real active-FAT32 read/write test with inactive-table write trap. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static u32 inactive_start,inactive_count,writes;
static int store(void*x,u32 l,const u8*b){(void)x;assert(l>=inactive_start+inactive_count||l<inactive_start);writes++;
 if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flush(void*x){(void)x;return fflush(file)?IO:0;}
static void verify(const char*p,u32 n,const u8*expected){MultiFile64 f;assert(!fs_open64(p,&f)&&f.size==n);u8 b[8193];assert(n<=sizeof b&&!fs_read64(&f,0,b,n)&&!memcmp(b,expected,n));}
int main(int argc,char**argv){assert(argc==5);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);
 inactive_start=(u32)strtoul(argv[2],0,0);inactive_count=(u32)strtoul(argv[3],0,0);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="active FAT32 regression",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=store,.flush=flush},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h));int result=fs_mount();
 if(!strcmp(argv[4],"deny")){if(result==0)assert(fs_unlock()<0);assert(!vol.writable&&!writes);fclose(file);puts("PASS rejected invalid geometry/mirror disagreement without writes");return 0;}
 assert(!result&&vol.kind==FS_FAT32);u8 expected[8193];memset(expected,'S',sizeof expected);verify("/SEED.BIN",sizeof expected,expected);
 u32 freeclusters;assert(!fs_free(&freeclusters)&&freeclusters>0);assert(!writes);
 if(!strcmp(argv[4],"read")){fclose(file);puts("PASS selected FAT read, cluster traversal and free space; zero writes");return 0;}
 assert(!fs_unlock()&&vol.writable);
 assert(!fs_put("/Созданный файл.txt",(const u8*)"new",3,0,0));
 assert(!fs_put("/SEED.BIN",(const u8*)"TAIL",4,2,0));
 assert(!fs_resize64("/SEED.BIN",sizeof expected));verify("/SEED.BIN",sizeof expected,expected);
 assert(!fs_write64("/SEED.BIN",509,(const u8*)"PATCH",5));memcpy(expected+509,"PATCH",5);verify("/SEED.BIN",sizeof expected,expected);
 assert(!fs_put("/COPY.BIN",0,0,4,"/SEED.BIN"));verify("/COPY.BIN",sizeof expected,expected);
 assert(!fs_rename("/COPY.BIN","RENAMED.BIN"));assert(!fs_put("/TESTDIR",0,0,3,0));assert(!fs_put("/TESTDIR/child",(const u8*)"child",5,0,0));
 assert(!fs_remove("/TESTDIR/child",0)&&!fs_remove("/TESTDIR",1));assert(!fs_remove("/Созданный файл.txt",0));
 assert(!fs_resize64("/RENAMED.BIN",513));assert(!fs_resize64("/RENAMED.BIN",8193));memset(expected+513,0,sizeof expected-513);verify("/RENAMED.BIN",sizeof expected,expected);
 assert(!disk_flush()&&!fs_mount()&&!vol.writable);verify("/RENAMED.BIN",sizeof expected,expected);fclose(file);
 printf("PASS active FAT32: read/free/create/append/copy/rename/random-write/resize/Unicode/directories/remount; %u sector writes, none to inactive FAT\n",writes);return 0;}
