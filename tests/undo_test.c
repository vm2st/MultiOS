/* MultiPulti | vm2 studios | t.me/v2vproj — sector fault-model tests, NOT filesystem certification. */
#include "multios/undo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#define N 512u
#define JS 16u
#define JN 18u
#define DS 256u
#define DN 64u
static u8 stable[N][512],cache[N][512],base[N][512],expected[N][512],pending[N][512],dirty[N];
static u8 a[512],b[512],c[512],d[512];
static MultiBlock64 io;static MultiUndo j;static MultiUndoConfig cfg;static jmp_buf poweroff;
static u32 steps,cut,mode,reads,writes,flushes,read_fail,positive,checks,crashes,errors,recoveries,corruptions;
static int high,return_error,hit;
static u32 silent_at,silent_mode,silent_hit;
#define CK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s steps=%u cut=%u mode=%u high=%d\n",__LINE__,#x,steps,cut,mode,high);assert(x);}}while(0)
void *mzero(void *p,u32 n){return memset(p,0,n);}void mcopy(void *p,const void *q,u32 n){memmove(p,q,n);}
static u32 crc(const u8 *p,u32 n){u32 v=~0u;while(n--){v^=*p++;for(u32 k=0;k<8;k++)v=v&1?(v>>1)^0xedb88320u:v>>1;}return ~v;}
static void p32(u8*p,u32 v){for(u32 i=0;i<4;i++){p[i]=v;v>>=8;}}
static void p64(u8*p,uint64_t v){p32(p,(u32)v);p32(p+4,(u32)(v>>32));}
static uint64_t g64(const u8*p){uint64_t v=0;for(u32 i=8;i;i--)v=(v<<8)|p[i-1];return v;}
static void fix(u8*p){p32(p+508,crc(p,508));}
static u32 physical(uint64_t lba){if(!high){CK(lba<N);return (u32)lba;}
 if(lba>=cfg.journal_start&&lba-cfg.journal_start<JN)return JS+(u32)(lba-cfg.journal_start);
 CK(lba>=cfg.data_start&&lba-cfg.data_start<DN);return DS+(u32)(lba-cfg.data_start);}
static void persist(void){for(u32 i=0;i<N;i++)if(dirty[i]){memcpy(stable[i],cache[i],512);dirty[i]=0;}}
static int stop(int writing,u32 p){hit=1;
 if(mode==1){if(writing)memcpy(stable[p],cache[p],173);else for(u32 i=0;i<N;i++)if(dirty[i])memcpy(stable[i],cache[i],173);}
 else if(mode==2){for(u32 i=0;i<N;i++)if(dirty[i]&&((i+steps)&1))memcpy(stable[i],cache[i],512);}
 else if(mode==3)persist();if(return_error)return IO;longjmp(poweroff,1);}
static int rd(void*x,uint64_t l,u8*out){(void)x;if(++reads==read_fail)return IO;memcpy(out,cache[physical(l)],512);return 0;}
static int wr(void*x,uint64_t l,const u8*in){(void)x;u32 p=physical(l);CK((l>=cfg.journal_start&&l-cfg.journal_start<cfg.journal_sectors)||(l>=cfg.data_start&&l-cfg.data_start<cfg.data_sectors));writes++;steps++;
 if(writes==silent_at){silent_hit=1;if(silent_mode==0)return 0;memcpy(cache[p],in,silent_mode==1?173:512);if(silent_mode==2)cache[p][233]^=0x5a;dirty[p]=1;return 0;}
 if(steps==positive)return 1;if(steps==cut&&mode==0)return stop(1,p);memcpy(cache[p],in,512);dirty[p]=1;if(steps==cut)return stop(1,p);return 0;}
static int fl(void*x){(void)x;flushes++;steps++;if(steps==cut)return stop(0,0);persist();return 0;}
static void reboot(void){memcpy(cache,stable,sizeof(cache));memset(dirty,0,sizeof(dirty));mzero(&io,sizeof(io));mzero(&j,sizeof(j));
 steps=cut=reads=writes=flushes=read_fail=positive=0;return_error=hit=0;silent_at=silent_mode=silent_hit=0;
 MultiBlock64Provider p={.abi=MULTIOS_BLOCK64_ABI,.sector_bytes=512,.sectors=high?((uint64_t)1<<40):N,.writable=1,.read=rd,.write=wr,.flush=fl};CK(!multi_block64_bind(&io,&p));}
static void init(void){cfg=(MultiUndoConfig){.journal_start=high?((uint64_t)1<<35):JS,.journal_sectors=JN,.data_start=high?((uint64_t)1<<36):DS,.data_sectors=DN,.media_id={1,7,3,8,5,9,4,3,2,8,6,1,8,0,3,4}};
 for(u32 i=0;i<N;i++)for(u32 k=0;k<512;k++)stable[i][k]=(u8)(i*19+k*7+(k>>4));memset(stable+JS,0,JN*512);reboot();CK(!multi_undo_format(&io,&cfg));
 memcpy(base,stable,sizeof(base));memcpy(expected,base,sizeof(expected));memset(a,0xa5,512);memset(b,0x5a,512);memset(c,0x91,512);memset(d,0x23,512);
 memcpy(expected[DS+2],c,512);memcpy(expected[DS+3],b,512);memcpy(expected[DS+5],d,512);reboot();CK(!multi_undo_open(&j,&io,&cfg));CK(j.ready&&!j.active&&j.capacity==8);}
static int transaction(int commit){TRY(multi_undo_begin(&j));TRY(multi_undo_write(&j,cfg.data_start+2,a));TRY(multi_undo_write(&j,cfg.data_start+3,b));
 TRY(multi_undo_write(&j,cfg.data_start+2,c));TRY(multi_undo_write(&j,cfg.data_start+5,d));return commit?multi_undo_commit(&j):multi_block64_flush(&io);}
static int try_tx(int commit){if(setjmp(poweroff))return IO;return transaction(commit);}
static int try_recover(void){if(setjmp(poweroff))return IO;return multi_undo_recover(&j);}
static int try_format(void){if(setjmp(poweroff))return IO;return multi_undo_format(&io,&cfg);}
static int image_same(const u8 x[N][512],const u8 y[N][512]){for(u32 i=0;i<N;i++)if(!(i>=JS&&i<JS+cfg.journal_sectors)&&memcmp(x[i],y[i],512))return 0;return 1;}
static void recover(int old_only){reboot();CK(!multi_undo_open(&j,&io,&cfg));CK(!multi_undo_recover(&j)&&!j.active);CK(image_same(stable,base)||(!old_only&&image_same(stable,expected)));
 u32 n=steps;CK(!multi_undo_recover(&j)&&steps==n);}
static void matrix(void){init();CK(!transaction(1));u32 total=steps;CK(total==34&&image_same(stable,expected)&&!j.active&&!j.count);
 for(u32 n=1;n<=total;n++)for(u32 m=0;m<4;m++)for(u32 error=0;error<2;error++){
  memcpy(stable,base,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=n;mode=m;return_error=error;
  CK(try_tx(1)<0&&hit);if(error){CK(j.failed&&io.failed);u32 before=steps;CK(multi_undo_write(&j,cfg.data_start,a)==READONLY&&steps==before);CK(multi_undo_commit(&j)==READONLY&&steps==before);errors++;}else crashes++;
  recover(0);
 }
 init();CK(!transaction(0)&&j.active&&j.count==3);memcpy(pending,stable,sizeof(pending));reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;CK(!multi_undo_recover(&j));total=steps;CK(total==8&&image_same(stable,base));
 for(u32 n=1;n<=total;n++)for(u32 m=0;m<4;m++)for(u32 error=0;error<2;error++){
  memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=n;mode=m;return_error=error;CK(try_recover()<0&&hit);recover(1);recoveries++;
 }
 memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=1;mode=1;CK(try_recover()<0);
 reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=5;mode=1;CK(try_recover()<0);recover(1);recoveries++;
}
static void corrupt(void){init();CK(!transaction(0));memcpy(pending,stable,sizeof(pending));
 for(u32 variant=0;variant<14;variant++){
  memcpy(stable,pending,sizeof(stable));u8*h=stable[JS],*h2=stable[JS+1],*desc=stable[JS+2];
  switch(variant){case 0:desc[100]^=1;break;case 1:stable[JS+3][123]^=1;break;
   case 2:p64(desc+24,cfg.journal_start);fix(desc);break;case 3:p64(desc+24,cfg.data_start+DN);fix(desc);break;
   case 4:p64(desc+16,g64(desc+16)+1);fix(desc);break;case 5:desc[40]^=2;fix(desc);break;
   case 6:p32(desc+12,1);fix(desc);break;case 7:desc[56]=1;fix(desc);break;
   case 8:p64(stable[JS+4]+24,cfg.data_start+2);fix(stable[JS+4]);break;
   case 9:p32(h+40,9);p32(h2+40,9);fix(h);fix(h2);break;
   case 10:h[0]^=2;h2[0]^=4;break;case 11:h[88]^=2;fix(h);break;
   case 12:p64(h+24,g64(h+24)+5);fix(h);break;case 13:p32(h+20,0);p32(h+40,0);fix(h);break;
  }
  reboot();CK(multi_undo_open(&j,&io,&cfg)==BAD&&!j.ready&&!writes&&!flushes);CK(image_same(stable,expected));corruptions++;
 }
 memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));cache[JS+3][123]^=1;CK(multi_undo_recover(&j)==BAD&&!writes);corruptions++;
 memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));cache[JS+3][123]^=1;CK(multi_undo_commit(&j)==BAD&&!writes&&!flushes&&j.failed);corruptions++;
 for(u32 n=1;n<=8;n++){memcpy(stable,pending,sizeof(stable));reboot();read_fail=n;CK(multi_undo_open(&j,&io,&cfg)==IO&&!writes&&io.failed);corruptions++;}
}
static void limits(void){init();CK(!multi_undo_begin(&j));for(u32 n=0;n<8;n++)CK(!multi_undo_write(&j,cfg.data_start+n,a));u32 before=steps;
 CK(j.count==8&&multi_undo_write(&j,cfg.data_start+8,a)==NOSPACE&&steps==before);CK(!multi_undo_write(&j,cfg.data_start,a)&&j.count==8);CK(!multi_undo_recover(&j)&&image_same(stable,base));
 before=writes;CK(multi_undo_format(&io,&cfg)==EXISTS&&writes==before);MultiUndoConfig bad=cfg;bad.data_start=cfg.journal_start;CK(multi_undo_format(&io,&bad)==INVALID);
 bad=cfg;bad.journal_start=UINT64_MAX-1;CK(multi_undo_format(&io,&bad)==INVALID);bad=cfg;bad.data_start=UINT64_MAX;CK(multi_undo_format(&io,&bad)==INVALID);
 bad=cfg;memset(bad.media_id,0,16);CK(multi_undo_format(&io,&bad)==INVALID);bad=cfg;bad.journal_sectors=100000;CK(multi_undo_format(&io,&bad)==INVALID);
 before=steps;CK(multi_undo_write(&j,cfg.data_start,a)==INVALID&&steps==before);CK(!multi_undo_begin(&j));before=steps;
 CK(multi_undo_write(&j,cfg.journal_start,a)==BAD&&steps==before);CK(multi_undo_write(&j,UINT64_MAX,a)==BAD&&steps==before);
 CK(multi_undo_write(&j,cfg.data_start+DN,a)==BAD&&steps==before);CK(multi_undo_write(&j,cfg.data_start,0)==INVALID&&steps==before);CK(!multi_undo_recover(&j)&&image_same(stable,base));
 init();steps=0;positive=1;CK(multi_undo_begin(&j)==IO&&j.failed&&io.failed);recover(1);
 init();CK(!transaction(0));reboot();io.provider.writable=0;CK(!multi_undo_open(&j,&io,&cfg));before=steps;CK(multi_undo_recover(&j)==READONLY&&steps==before);
 reboot();io.provider.sector_bytes=4096;CK(multi_undo_open(&j,&io,&cfg)==INVALID);
 memcpy(stable,base,sizeof(stable));for(u32 i=0;i<2;i++){p64(stable[JS+i]+24,UINT64_MAX);fix(stable[JS+i]);}reboot();CK(!multi_undo_open(&j,&io,&cfg));before=steps;CK(multi_undo_begin(&j)==UNSUP&&steps==before);
 /* Last usable sequence: a full transaction can still commit, then no wrap. */
 memcpy(stable,base,sizeof(stable));for(u32 i=0;i<2;i++){p64(stable[JS+i]+24,UINT64_MAX-9);fix(stable[JS+i]);}
 reboot();CK(!multi_undo_open(&j,&io,&cfg));before=steps;CK(multi_undo_begin(&j)==UNSUP&&steps==before);
 for(u32 i=0;i<2;i++){p64(stable[JS+i]+24,UINT64_MAX-10);fix(stable[JS+i]);}
 reboot();CK(!multi_undo_open(&j,&io,&cfg));CK(!multi_undo_begin(&j));for(u32 i=0;i<8;i++)CK(!multi_undo_write(&j,cfg.data_start+i,a));
 CK(!multi_undo_commit(&j)&&j.sequence==UINT64_MAX);before=steps;CK(multi_undo_begin(&j)==UNSUP&&steps==before);
 /* Configuration may alias an unready object's config, without erasing it. */
 init();reboot();j.config=cfg;CK(!multi_undo_open(&j,&io,&j.config)&&j.ready&&j.config.data_start==cfg.data_start);
 /* A forged exhausted ACTIVE header must not permit repeated-sector writes. */
 init();CK(!transaction(0));for(u32 i=0;i<2;i++){p64(stable[JS+i]+24,UINT64_MAX);fix(stable[JS+i]);}
 reboot();CK(!multi_undo_open(&j,&io,&cfg));before=steps;CK(multi_undo_write(&j,cfg.data_start+2,a)==UNSUP&&steps==before);
}
static void formatting(void){init();for(u32 n=1;n<=4;n++)for(u32 m=0;m<4;m++){
 memcpy(stable,base,sizeof(stable));memset(stable+JS,0,JN*512);reboot();cut=n;mode=m;CK(try_format()<0&&hit);reboot();int r=multi_undo_open(&j,&io,&cfg);CK(r==BAD||r==0);if(!r)CK(!j.active&&!multi_undo_recover(&j));CK(image_same(stable,base));}}
static void capacity_extremes(void){
 high=0;
 for(u32 cap=1;cap<=128;cap+=127){
  init();cfg.journal_sectors=2+2*cap;cfg.data_start=383;cfg.data_sectors=129;
  memset(stable+JS,0,(size_t)cfg.journal_sectors*512);reboot();CK(!multi_undo_format(&io,&cfg));memcpy(base,stable,sizeof(base));
  reboot();CK(!multi_undo_open(&j,&io,&cfg)&&j.capacity==cap);CK(!multi_undo_begin(&j));
  for(u32 n=0;n<cap;n++)CK(!multi_undo_write(&j,cfg.data_start+n,a));
  u32 before=steps;CK(multi_undo_write(&j,cfg.data_start+cap,a)==NOSPACE&&steps==before);
  CK(!multi_block64_flush(&io));reboot();CK(!multi_undo_open(&j,&io,&cfg)&&j.count==cap);CK(!multi_undo_recover(&j)&&image_same(stable,base));
  CK(!multi_undo_begin(&j));for(u32 n=0;n<cap;n++)CK(!multi_undo_write(&j,cfg.data_start+n,b));CK(!multi_undo_commit(&j));
  reboot();CK(!multi_undo_open(&j,&io,&cfg)&&!j.active);for(u32 n=0;n<cap;n++)CK(!memcmp(stable[383+n],b,512));
 }
}
int main(void){CK(crc((const u8*)"123456789",9)==0xcbf43926);for(high=0;high<2;high++){matrix();corrupt();limits();formatting();}capacity_extremes();
 printf("PASS private undo v1: %u checks; %u transaction crash cases; %u I/O error cases; %u recovery interruption cases; %u corruption/read-failure cases.\n",checks,crashes,errors,recoveries,corruptions);
 puts("Models: lost cache, torn sectors, unordered partial persistence, completed flush; low and >32-bit LBAs; repeat-sector undo, bounded capacity, wrong binding, corrupt journal, read-only and sequence-overflow guards.");
 puts("Standalone unit test, NOT a filesystem RW test or NTFS $LogFile; integrated profile tested by test-storage-tx. NOT hardware/power-loss certification.");}
