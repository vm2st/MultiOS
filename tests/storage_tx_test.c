/* MultiPulti | vm2 studios | t.me/v2vproj — REAL filesystems + RAM fault model.
 * Synthetic persistence, not hardware certification; backend selected at build.
 */
#include "multios/hal.h"
#include "multios/storage_tx.h"
#include "multios/undo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
#define MAX 16384u
typedef struct {u32 lba;u8 bytes[512];} Sector;
static u8 *base,*stable,*changed;
static u32 sectors,jstart,tracked[MAX],ntracked,ncache,nexpected,npending;
static Sector cache[MAX],expected[MAX],pending[MAX];
static jmp_buf off;
static u32 step,cut,mode,as_error,hit,checks,crashes,errors,recoveries;
static int ro;static u32 extra,budget;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s step=%u cut=%u mode=%u error=%u\n",__LINE__,#x,step,cut,mode,as_error);abort();}}while(0)
static u32 u32le(const u8*p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void mark(u32 l){if(!changed[l]){CK(ntracked<MAX);changed[l]=1;tracked[ntracked++]=l;}}
static void save_one(u32 i,u32 n){u32 l=cache[i].lba;mark(l);memcpy(stable+(size_t)l*512,cache[i].bytes,n);}
static void persist(void){for(u32 i=0;i<ncache;i++)save_one(i,512);ncache=0;}
static int stop(int writing,u32 index){hit=1;
 if(mode==1){if(writing)save_one(index,173);else for(u32 i=0;i<ncache;i++)save_one(i,173);}
 else if(mode==2){for(u32 i=0;i<ncache;i++)if((cache[i].lba+step)&1)save_one(i,512);}
 else if(mode==3)persist();
 if(as_error)return IO;longjmp(off,1);
}
static int read_sector(void*x,u32 l,u8*b){(void)x;CK(l<sectors);for(u32 i=0;i<ncache;i++)if(cache[i].lba==l){memcpy(b,cache[i].bytes,512);return 0;}memcpy(b,stable+(size_t)l*512,512);return 0;}
static int write_sector(void*x,u32 l,const u8*b){(void)x;CK(!ro&&l<sectors);step++;
 if(step==cut&&mode==0)return stop(1,0);
 u32 i=0;while(i<ncache&&cache[i].lba!=l)i++;
 if(i==ncache){CK(ncache<MAX);cache[ncache++].lba=l;}
 memcpy(cache[i].bytes,b,512);if(step==cut)return stop(1,i);return 0;}
static int flush_sector(void*x){(void)x;step++;if(step==cut)return stop(0,0);persist();return 0;}
static void output(void*x,char c){(void)x;(void)c;}
static int input(void*x){(void)x;return -1;}
int multi_osk_process(int key){return key;}
static void bind_device(void){ncache=0;step=cut=hit=0;as_error=0;mzero(&vol,sizeof(vol));vol.kind=-1;
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="RAM fault model",.architecture="host",.block={.sectors=sectors+extra,.sector_bytes=512,.writable=!ro,.read=read_sector,.write=write_sector,.flush=flush_sector},.console={.putc=output,.getc=input}};
 CK(!multi_hal_bind(&h));}
static void reset_image(void){for(u32 i=0;i<ntracked;i++){u32 l=tracked[i];memcpy(stable+(size_t)l*512,base+(size_t)l*512,512);changed[l]=0;}ntracked=0;ro=0;extra=0;bind_device();}
static void mount_rw(void){CK(!fs_mount());CK(!fs_unlock()&&vol.writable);step=0;}
static int action(u32 which){static u8 large[(MULTIOS_UNDO_MAX_RECORDS+1)*512];
 switch(which){case 0:return fs_put("/Созданный файл.txt",(const u8*)"new data",8,0,0);
 case 1:return fs_put("/old.txt",(const u8*)"replacement",11,1,0);
 case 2:return fs_put("/old.txt",(const u8*)" appended",9,2,0);
 case 3:return fs_put("/copied.txt",0,0,4,"/old.txt");
 case 4:return fs_rename("/old.txt","переименован.txt");
 case 5:return fs_remove("/old.txt",0);
 case 6:return fs_put("/directory",0,0,3,0);
 case 7:return fs_remove("/empty",1);
 default:return fs_put("/too-large.bin",large,(budget+1)*512,0,0);}
}
static int try_action(u32 which){if(setjmp(off))return IO;return action(which);}
static int try_mount(void){if(setjmp(off))return IO;return fs_mount();}
static int isjournal(u32 l){return l>=jstart&&l<jstart+2+2*budget;}
static const u8*expected_sector(u32 l){for(u32 i=0;i<nexpected;i++)if(expected[i].lba==l)return expected[i].bytes;return base+(size_t)l*512;}
static void compare(int old_only){int old=1,new=1;
 for(u32 i=0;i<ntracked;i++){u32 l=tracked[i];if(isjournal(l))continue;const u8*p=stable+(size_t)l*512;
  if(memcmp(p,base+(size_t)l*512,512))old=0;if(memcmp(p,expected_sector(l),512))new=0;}
 for(u32 i=0;i<nexpected;i++)if(memcmp(stable+(size_t)expected[i].lba*512,expected[i].bytes,512))new=0;
 CK(old||(!old_only&&new));
}
static void recover_mount(int old_only){bind_device();CK(!fs_mount()&&vol.mounted&&!vol.writable);compare(old_only);CK(!fs_unlock());CK(!disk_flush());}
static void matrix(void){
 for(u32 op=0;op<8;op++){
  reset_image();mount_rw();CK(!action(op));u32 total=step;CK(total>4);nexpected=0;
  for(u32 i=0;i<ntracked;i++){u32 l=tracked[i];if(isjournal(l)||!memcmp(stable+(size_t)l*512,base+(size_t)l*512,512))continue;
   CK(nexpected<MAX);expected[nexpected].lba=l;memcpy(expected[nexpected++].bytes,stable+(size_t)l*512,512);}
  CK(nexpected);printf("operation %u: %u write/flush boundaries, %u changed non-journal sectors\n",op,total,nexpected);fflush(stdout);
  for(u32 n=1;n<=total;n++)for(u32 m=0;m<4;m++)for(u32 err=0;err<2;err++){
   reset_image();mount_rw();cut=n;mode=m;as_error=err;CK(try_action(op)<0&&hit);
   if(err){CK(!vol.mounted&&!vol.writable&&multi_storage_blocked());u32 before=step;CK(disk_write(2048,base)==READONLY&&step==before);errors++;}else crashes++;
   recover_mount(0);
  }
 }
 /* Capacity failure is an aborted operation, never a partially committed file. */
 reset_image();mount_rw();int capacity_result=action(8);
 if(capacity_result!=NOSPACE||vol.mounted||vol.writable||multi_storage_blocked())
  fprintf(stderr,"capacity: budget=%u result=%d mounted=%u writable=%u blocked=%d\n",budget,capacity_result,vol.mounted,vol.writable,multi_storage_blocked());
 CK(capacity_result==NOSPACE&&!vol.mounted&&!vol.writable&&!multi_storage_blocked());nexpected=0;compare(1);recover_mount(1);
 /* Validation failures also leave no changes and invalidate cached FS state. */
 reset_image();mount_rw();CK(fs_put("/old.txt",0,0,0,0)==EXISTS&&!vol.mounted);compare(1);recover_mount(1);
}
static void recovery_matrix(void){
 reset_image();mount_rw();CK(!action(0));u32 total=step;
 reset_image();mount_rw();cut=total-3;mode=0;CK(try_action(0)<0&&hit);npending=ntracked;CK(npending<MAX);
 for(u32 i=0;i<npending;i++){pending[i].lba=tracked[i];memcpy(pending[i].bytes,stable+(size_t)tracked[i]*512,512);}
 nexpected=0;ro=1;bind_device();CK(fs_mount()==READONLY&&!vol.mounted&&!step);ro=0;
 bind_device();CK(!fs_mount());total=step;CK(total>4);compare(1);
 for(u32 n=1;n<=total;n++)for(u32 m=0;m<4;m++)for(u32 err=0;err<2;err++){
  reset_image();for(u32 i=0;i<npending;i++){u32 l=pending[i].lba;mark(l);memcpy(stable+(size_t)l*512,pending[i].bytes,512);}
  bind_device();cut=n;mode=m;as_error=err;CK(try_mount()<0&&hit);recover_mount(1);recoveries++;
 }
 /* Recovery interrupted twice: torn target, then torn first CLEAN mirror. */
 reset_image();for(u32 i=0;i<npending;i++){u32 l=pending[i].lba;mark(l);memcpy(stable+(size_t)l*512,pending[i].bytes,512);}
 bind_device();cut=1;mode=1;CK(try_mount()<0&&hit);
 bind_device();cut=total-3;mode=1;CK(try_mount()<0&&hit);recover_mount(1);recoveries++;
 printf("recovery: %u write/flush boundaries checked\n",total);
}
static void guards(void){
 reset_image();ro=1;bind_device();CK(!fs_mount());CK(fs_unlock()==READONLY);u32 before=step;CK(fs_put("/denied",0,0,0,0)==READONLY&&step==before);
 reset_image();mark(sectors-1);stable[(size_t)(sectors-1)*512+100]^=1;bind_device();CK(fs_mount()==BAD&&!vol.mounted&&!step);
 reset_image();mark(0);stable[440]^=1;bind_device();CK(fs_mount()==BAD&&!vol.mounted&&!step);
 reset_image();mark(sectors-1);mark(sectors-2);memset(stable+(size_t)(sectors-2)*512,0,1024);bind_device();CK(!fs_mount());CK(fs_unlock()==READONLY);
 reset_image();mount_rw();before=step;CK(disk_write(vol.base,base)==READONLY&&step==before); /* no transaction bypass */
 /* Physical stick larger than image: MBR locator finds original journal. */
 reset_image();extra=8192;bind_device();CK(!fs_mount());CK(!fs_unlock());CK(!action(0));
 reset_image();
}
int main(int argc,char**argv){CK(argc==2);FILE*f=fopen(argv[1],"rb");CK(f);CK(!fseek(f,0,SEEK_END));long size=ftell(f);CK(size>0&&size%512==0);rewind(f);sectors=(u32)(size/512);
 base=malloc((size_t)size);stable=malloc((size_t)size);changed=calloc(sectors,1);CK(base&&stable&&changed);CK(fread(base,1,(size_t)size,f)==(size_t)size);fclose(f);memcpy(stable,base,(size_t)size);
 CK(!memcmp(base+(size_t)(sectors-1)*512,"MPSTORE1",8));jstart=u32le(base+(size_t)(sectors-1)*512+24);budget=u32le(base+(size_t)jstart*512+44);CK(budget>0&&budget<=MULTIOS_UNDO_MAX_RECORDS);matrix();recovery_matrix();guards();
 printf("PASS real-FS private transactions: %u assertions; %u crash cases; %u I/O cases; %u recovery interruption cases. NOT hardware certification; see compiled profile and input FS.\n",checks,crashes,errors,recoveries);
 free(base);free(stable);free(changed);return 0;}
