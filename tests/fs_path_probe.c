/* MultiOS — mutation paths must never be silently normalized by a backend. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include <string.h>
static u32 writes;
static MultiFile64 intact;
static int writeback(void*x,u32 l,const u8*b){(void)x;writes++;if(l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET))return IO;return fwrite(b,1,512,file)==512?0:IO;}
static int flushback(void*x){(void)x;return fflush(file)?IO:0;}
static void mountdisk(void){MultiHal h={.abi=MULTIOS_HAL_ABI,.name="path safety test",.architecture="host",.block={.sectors=sectors,.sector_bytes=512,.writable=1,.read=rd,.write=writeback,.flush=flushback},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h)&&!fs_mount()&&!fs_unlock());}
static void verify(const char*p,const u8*b,u32 n){MultiFile64 f;u8 actual[16];assert(n<=sizeof actual&&!fs_open64(p,&f)&&f.size==n&&!fs_read64(&f,0,actual,n)&&!memcmp(actual,b,n));}
static void untouched(const char *path,int result,u32 before){
 if(result!=INVALID||writes!=before){fprintf(stderr,"path=%s result=%d sector_writes=%u\n",path,result,writes-before);assert(0);}
 assert(vol.mounted&&vol.writable&&!vol.poisoned);
 u8 c;assert(!fs_read64(&intact,0,&c,1)&&c=='K');
 verify("/KEEP.TXT",(const u8*)"KEEP",4);
}
int main(int argc,char **argv){assert(argc==2);file=fopen(argv[1],"r+b");assert(file);assert(!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();
 assert(!fs_put("/KEEP.TXT",(const u8*)"KEEP",4,0,0));
 assert(!fs_put("/DIR",0,0,3,0));assert(!fs_put("/DIR/CHILD",(const u8*)"child",5,0,0));
 assert(!fs_open64("/KEEP.TXT",&intact));
 const char *bad[]={"/KEEP.TXT.","/KEEP.TXT ","//KEEP.TXT","/DIR/../KEEP.TXT","/./KEEP.TXT","/KEEP.TXT/","KEEP.TXT","/DIR//CHILD","/DIR/CHILD.","/DIR/CHILD ","/DIR/./CHILD","/DIR/../NEW","/BAD\\NAME","/BAD:NAME","/BAD*NAME","/BAD?NAME","/BAD\x01NAME","/BAD\xffNAME","/DIR./CHILD","/DIR /CHILD","/"};
 if(getenv("MULTIOS_PATH_REPRO")){
  int r=fs_put("/KEEP.TXT.",(const u8*)"LOST",4,1,0);u8 bytes[4];MultiFile64 f;assert(!fs_open64("/KEEP.TXT",&f)&&!fs_read64(&f,0,bytes,4));
  printf("replacement through trailing dot: result=%d stored=%.4s\n",r,bytes);fclose(file);return 0;
 }
 for(u32 i=0;i<sizeof bad/sizeof *bad;i++){
  for(int op=0;op<13;op++){
   u32 before=writes;int r=op<=3?fs_put(bad[i],(const u8*)"BAD",3,op,0):op==4?fs_remove(bad[i],0):op==5?fs_remove(bad[i],1):op==6?fs_rename(bad[i],"RENAMED"):op==7?fs_put("/COPY",0,0,4,bad[i]):op==8?fs_write64(bad[i],0,(const u8*)"X",1):op==9?fs_resize64(bad[i],1):op==10?fs_stream_change(bad[i],"Notes",MULTIOS_STREAM_CREATE,0,0,0):op==11?fs_link("/KEEP.TXT",bad[i]):fs_link(bad[i],"/LINK");
   untouched(bad[i],r,before);
  }
 }
 const char *leaves[]={"KEEP.TXT.","KEEP.TXT ",".","..","BAD/NAME","BAD\\NAME","BAD:NAME","BAD*NAME","BAD?NAME","BAD\xffNAME",""};
 for(u32 i=0;i<sizeof leaves/sizeof *leaves;i++){u32 before=writes;untouched(leaves[i],fs_rename("/KEEP.TXT",leaves[i]),before);}
 char overlong[PATHMAX+1];memset(overlong,'a',PATHMAX);overlong[0]='/';overlong[PATHMAX]=0;u32 before=writes;
 assert(fs_put(overlong,(const u8*)"X",1,1,0)==TOOLONG&&writes==before);
 /* Legitimate case fallback, spaces INSIDE names, Unicode and nesting remain usable. */
 assert(!fs_put("/DIR/имя файла.txt",(const u8*)"ok",2,0,0));
 assert(!fs_put("/keep.txt",(const u8*)"!",1,2,0));verify("/KEEP.TXT",(const u8*)"KEEP!",5);
 assert(!fs_rename("/DIR/имя файла.txt","другое имя.txt"));verify("/DIR/другое имя.txt",(const u8*)"ok",2);
 assert(!disk_flush());fclose(file);puts("PASS invalid mutation/source paths and rename leaves: no sector writes, no unmount, protected bytes unchanged; normal case/Unicode/nested paths work");return 0;
}
