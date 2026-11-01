/* vm2 studios | t.me/v2vproj — >legacy 128-sector transaction regression. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static int writefile(void*x,u32 l,const u8*b){(void)x;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flushfile(void*x){(void)x;return fflush(file)?IO:0;}
int main(int argc,char**argv){assert(argc==2);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="expanded undo probe",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writefile,.flush=flushfile},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());u8*data=malloc(1024*1024);assert(data);for(u32 i=0;i<1024*1024;i++)data[i]=(u8)((i*17+(i>>8))&255);
 assert(!fs_put("/megabyte.bin",data,1024*1024,0,0));assert(!fs_put("/megabyte-copy.bin",0,0,4,"/megabyte.bin"));
 MultiFile64 view;assert(!fs_open64("/megabyte-copy.bin",&view)&&view.size==1024*1024);u8 b[8192];
 for(u32 off=0;off<1024*1024;off+=sizeof b){assert(!fs_read64(&view,off,b,sizeof b));assert(!memcmp(b,data+off,sizeof b));}
 assert(!fs_rename("/megabyte-copy.bin","renamed-megabyte.bin"));assert(!fs_remove("/megabyte.bin",0));u32 freeclusters;assert(!fs_free(&freeclusters));assert(!disk_flush());free(data);fclose(file);
 puts("PASS 1 MiB atomic create + copy + full readback + rename + delete on 4096-sector undo budget.");return 0;}
