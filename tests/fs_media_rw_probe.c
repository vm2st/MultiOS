/* MultiOS: real VFS mutations, selected-media loss before/after raw writes.
 * Not an actual USB removal or power-loss test. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include "multios/storage_tx.h"
#include <string.h>
static int cookie,phase,fired;static u32 writes,fail_at,calls;
static int readback(void*x,u32 l,u8*b){assert(x==&cookie);calls++;return rd(0,l,b);}
static void notify(void){fired=1;assert(!multi_hal_media_failed(&cookie));}
static int writeback(void*x,u32 l,const u8*b){assert(x==&cookie);calls++;writes++;
 if(writes==fail_at&&!phase){notify();return 0;}
 int r=l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET)?IO:fwrite(b,1,512,file)==512?0:IO;
 if(writes==fail_at&&phase)notify();return r;
}
static int flushback(void*x){assert(x==&cookie);calls++;return fflush(file)?IO:0;}
static void mountdisk(void){MultiHal h={.abi=MULTIOS_HAL_ABI,.name="VFS media failure model",.architecture="host",.block={.ctx=&cookie,.sectors=sectors,.sector_bytes=512,.writable=1,.read=readback,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static u8 original[32769],expected[65539],actual[65539];static const u8 patch[9]="PATCH123";
int main(int argc,char**argv){assert(argc>=3);file=fopen(argv[1],"r+b");assert(file&&!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();
 for(u32 i=0;i<sizeof original;i++)original[i]=(u8)(i*37+11);memcpy(expected,original,sizeof original);
 if(!strcmp(argv[2],"seed")){for(u32 o=0;o<sizeof original;){u32 n=sizeof original-o;if(n>8192)n=8192;assert(!fs_put("/DATA.BIN",original+o,n,o?2:0,0));o+=n;}}
 else if(!strcmp(argv[2],"attempt")){assert(argc==6);fail_at=strtoul(argv[3],0,0);phase=atoi(argv[4]);int op=atoi(argv[5]);writes=0;
  int r=op?fs_resize64("/DATA.BIN",65539):fs_write64("/DATA.BIN",509,patch,sizeof patch);
  if(!fail_at)assert(!r);else{assert(fired&&r<0&&!vol.mounted&&!vol.writable&&multi_storage_blocked());u32 before=calls;u8 b[512];
   assert(disk_read(0,b)<0&&disk_write(0,b)<0&&disk_flush()<0);
   assert(multi_storage_read(0,b)<0&&multi_storage_flush()<0&&multi_storage_prepare()<0&&multi_storage_begin()<0);
   assert(before==calls);
  }
  printf("result=%d writes=%u\n",r,writes);
 }
 else if(!strcmp(argv[2],"recover")){assert(argc==4);int op=atoi(argv[3]);MultiFile64 f;assert(!fs_open64("/DATA.BIN",&f));
  assert(f.size==sizeof original||(op&&f.size==sizeof expected));assert(!fs_read64(&f,0,actual,(u32)f.size));
  if(op){assert(!memcmp(actual,expected,(u32)f.size));}
  else{memcpy(expected+509,patch,sizeof patch);assert(!memcmp(actual,original,sizeof original)||!memcmp(actual,expected,sizeof original));}
  /* A fresh healthy mount must also be able to begin a new FS transaction. */
  assert(!fs_put("/RECOVER.TXT",(const u8*)"OK",2,0,0));
 }
 else assert(0);
 if(!fired)assert(!disk_flush());fclose(file);return 0;
}
