/* vm2 studios | t.me/v2vproj — compressed and >4GiB NTFS mutation regression. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static int writefile(void*x,u32 l,const u8*b){(void)x;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flushfile(void*x){(void)x;return fflush(file)?IO:0;}
int main(int argc,char**argv){assert(argc==2);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="NTFS compressed RW probe",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writefile,.flush=flushfile},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());static u8 data[32768],result[65536];for(u32 i=0;i<sizeof data;i++)data[i]=(u8)(i%17);
 assert(!fs_put("/COMPRESSED/lznt1.bin",data,sizeof data,1,0));assert(!fs_put("/COMPRESSED/lznt1.bin",data,sizeof data,2,0));
 MultiFile64 view;assert(!fs_open64("/COMPRESSED/lznt1.bin",&view)&&view.size==65536);assert(!fs_read64(&view,0,result,65536));assert(!memcmp(result,data,32768)&&!memcmp(result+32768,data,32768));
 int copy_result=fs_put("/COMPRESSED/copy.bin",0,0,4,"/COMPRESSED/lznt1.bin");fprintf(stderr,"compressed copy result=%d\n",copy_result);assert(!copy_result);assert(!fs_rename("/COMPRESSED/copy.bin","renamed.bin"));
 assert(!fs_open64("/COMPRESSED/renamed.bin",&view)&&view.size==65536);assert(!fs_read64(&view,0,result,65536));assert(!memcmp(result,data,32768)&&!memcmp(result+32768,data,32768));assert(!fs_remove("/COMPRESSED/renamed.bin",0));
 assert(!fs_put("/huge-sparse.bin",(const u8*)"MORE",4,2,0));assert(!fs_open64("/huge-sparse.bin",&view)&&view.size==5ULL*1024*1024*1024+8);
 assert(!fs_read64(&view,view.size-8,result,8)&&!memcmp(result,"END!MORE",8));u32 freeclusters;assert(!fs_free(&freeclusters));assert(!disk_flush());fclose(file);
 puts("PASS NTFS compressed replace/append/copy/rename/delete, compressed readback, >5GiB sparse append, free-space query.");return 0;}
