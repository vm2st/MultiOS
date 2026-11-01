/* vm2 studios | t.me/v2vproj — real upstream directory growth + in-place append. */
#define _POSIX_C_SOURCE 200809L
#include "multios/hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static FILE*f;static u32 blocks;static unsigned writes;
static int rd(void*x,u32 l,u8*b){(void)x;if(l>=blocks||fseeko(f,(off_t)l*512,SEEK_SET))return IO;return fread(b,1,512,f)==512?0:IO;}
static int wr(void*x,u32 l,const u8*b){(void)x;writes++;if(l>=blocks||fseeko(f,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,f)==512?0:IO;}
static int flush(void*x){(void)x;return fflush(f)?IO:0;}
static void put(void*x,char c){(void)x;(void)c;}
static int get(void*x){(void)x;return -1;}
int multi_osk_process(int k){return k;}
int main(int argc,char**argv){assert(argc==2);f=fopen(argv[1],"r+b");assert(f);assert(!fseeko(f,0,SEEK_END));blocks=(u32)(ftello(f)/512);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="growth test",.architecture="host",.block={.sectors=blocks,.sector_bytes=512,.writable=1,.read=rd,.write=wr,.flush=flush},.console={.putc=put,.getc=get}};
 assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());
 unsigned n;char path[128];for(n=0;n<120;n++){snprintf(path,sizeof path,"/long filename for directory growth number %03u.txt",n);assert(!fs_put(path,(const u8*)"growth",6,0,0));}
 assert(!fs_put("/тест.txt",(const u8*)"cp866",5,0,0));
 assert(!fs_put("/emoji-😀.txt",(const u8*)"unicode",7,0,0));
 static u8 data[8192];for(u32 i=0;i<sizeof data;i++)data[i]=(u8)i;
 for(n=0;n<20;n++)assert(!fs_put("/large.bin",data,sizeof data,n?2:0,0));
 writes=0;assert(!fs_put("/large.bin",(const u8*)"TAIL",4,2,0));unsigned append_writes=writes;
 MultiFile64 view;assert(!fs_open64("/large.bin",&view)&&view.size==20*sizeof data+4);
 u8 buf[8192];for(n=0;n<20;n++){assert(!fs_read64(&view,n*sizeof data,buf,sizeof buf));assert(!memcmp(buf,data,sizeof data));}
 assert(!fs_read64(&view,20*sizeof data,buf,4)&&!memcmp(buf,"TAIL",4));
 assert(!fs_open64("/ТЕСТ.TXT",&view));assert(!fs_read64(&view,0,buf,5)&&!memcmp(buf,"cp866",5));
 assert(!fs_unlock()); /* Independent native audit after upstream directory growth. */
 assert(!disk_flush());fclose(f);printf("PASS upstream growth: FS=%d; 120 long files + OEM CP866 + nonBMP; 160 KiB append built incrementally; tail append %u raw writes incl undo; post-growth RW audit passed.\n",vol.kind,append_writes);return 0;
}
