/* MultiPulti | vm2 studios | t.me/v2vproj
 * Opt-in adapter: complete VFS mutation -> one private undo transaction.
 * Not NTFS $LogFile. No direct disk-write bypass, automatic format or repair.
 */
#include "multios/storage_tx.h"
#include "multios/undo.h"
#include "multios/hal.h"
static MultiBlock64 device;
static MultiUndo journal;
static int state; /* 0 unopened; 1 no locator (RO only); 2 ready; -1 blocked */
static const u8 magic[8]={'M','P','S','T','O','R','E','1'};
static u32 get32(const u8*p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static uint64_t get64(const u8*p){return get32(p)|((uint64_t)get32(p+4)<<32);}
static int equal(const u8*a,const u8*b,u32 n){while(n--)if(*a++!=*b++)return 0;return 1;}
static u32 crc(const u8*p,u32 n){u32 c=UINT32_MAX;while(n--){c^=*p++;for(u32 k=0;k<8;k++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int zero(const u8*p,u32 n){while(n--)if(*p++)return 0;return 1;}
/* The provider can report selected-media loss during a synchronous callback.
 * Do not let its later zero result erase that sticky notification. */
static int raw_result(int r){if(r>0)r=IO;return !r&&(multi_hal_media_unavailable()||state<0||device.failed)?READONLY:r;}
static int raw_read(void*ctx,uint64_t lba,u8*b){const MultiHal*h=ctx;if(!h||!b)return INVALID;if(lba>=h->block.sectors)return BAD;return raw_result(h->block.read(h->block.ctx,(u32)lba,b));}
static int raw_write(void*ctx,uint64_t lba,const u8*b){const MultiHal*h=ctx;if(!h||!b)return INVALID;if(lba>=h->block.sectors||!h->block.writable||!h->block.write)return READONLY;return raw_result(h->block.write(h->block.ctx,(u32)lba,b));}
static int raw_flush(void*ctx){const MultiHal*h=ctx;if(!h)return INVALID;return raw_result(h->block.flush?h->block.flush(h->block.ctx):(h->block.writable?IO:0));}
/* Keep the journal and ACTIVE record intact; only an explicit fresh binding may
 * reopen/recover it. In particular do not attempt rollback on a removed disk. */
void multi_storage_media_failed(void){state=-1;device.failed=1;device.provider.writable=0;}
void multi_storage_reset(void){mzero(&device,sizeof(device));mzero(&journal,sizeof(journal));state=0;}
int multi_storage_blocked(void){return multi_hal_media_unavailable()||state<0||device.failed||journal.failed;}
const char *multi_storage_description(void){
 if(multi_storage_blocked())return "Private undo: BLOCKED; reopen device/reboot before recovery";
 if(state==1)return "Private undo: no prepared locator; RW refused";
 if(state!=2)return "Private undo: not opened";
 if(!multi_storage_matches(vol.base,vol.sectors))return "Private undo: selected partition not protected; RW refused";
 if(journal.degraded)return "Private undo: degraded mirrors; valid copy selected; older/invalid copy replaced first on next transaction";
 return "Private undo v1: whole-operation, configured budget <=4096 sectors; EXPERIMENTAL";
}
static int fail(int r){state=-1;return r;}
static void unmount_failed(void){vol.mounted=0;vol.writable=0;vol.poisoned=1;vol.kind=-1;}
/* The immutable footer binds an explicit spare tail to one MBR partition.
 * All partitions must be disjoint, bounded and entirely BEFORE that tail.
 * No GPT, EBR, filesystem-internal journal file, or scavenged free sectors.
 */
static int locator(const u8*a,const u8*b,MultiUndoConfig*c){
 if(!equal(a,magic,8)&&!equal(b,magic,8))return NOTFOUND;
 if(!equal(a,b,512)||!equal(a,magic,8)||get32(a+508)!=crc(a,508)||get32(a+8)!=1||get32(a+12)!=512||!zero(a+80,428))return BAD;
 if(get64(a+16)!=device.provider.sectors)return BAD;
 c->journal_start=get64(a+24);c->journal_sectors=get64(a+32);c->data_start=get64(a+40);c->data_sectors=get64(a+48);mcopy(c->media_id,a+56,16);
 if((c->journal_sectors<4||c->journal_sectors>2+2*MULTIOS_UNDO_MAX_RECORDS||(c->journal_sectors&1))||c->journal_start<2048||c->journal_start>device.provider.sectors||
    device.provider.sectors-c->journal_start!=c->journal_sectors+2||!c->data_start||!c->data_sectors||
    c->data_start>=c->journal_start||c->data_sectors>c->journal_start-c->data_start)return BAD;
 u32 part=get32(a+72);if(part<1||part>4)return BAD;
 u8 mbr[512];TRY(multi_block64_read(&device,0,mbr));if(mbr[510]!=0x55||mbr[511]!=0xaa||get32(a+76)!=crc(mbr,512))return BAD;
 u32 starts[4]={0},lengths[4]={0},journal_parts=0;
 for(u32 i=0;i<4;i++){
  const u8*p=mbr+446+i*16;u32 start=get32(p+8),n=get32(p+12);u8 type=p[4];
  if(!type){if(start||n)return BAD;continue;}
  if(type==0xda){
   if(++journal_parts>1||i==part-1||start!=c->journal_start||n!=c->journal_sectors+2||p[0])return BAD;
   continue;
  }
  if(type==0xee||type==5||type==0x0f||type==0x85||!start||!n||start>=c->journal_start||n>c->journal_start-start)return BAD;
  starts[i]=start;lengths[i]=n;
  for(u32 k=0;k<i;k++)if(lengths[k]&&start<starts[k]+lengths[k]&&starts[k]<start+n)return BAD;
 }
 if(starts[part-1]!=c->data_start||lengths[part-1]!=c->data_sectors)return BAD;
 return 0;
}
int multi_storage_prepare(void){
 if(multi_hal_media_unavailable())return fail(READONLY);
 if(state==2)return multi_storage_blocked()?READONLY:0;
 if(state==1)return 0;if(state<0)return READONLY;
 const MultiHal*h=multi_hal();if(!h||h->block.sectors<2)return fail(INVALID);
 /* A dedicated MBR 0xDA extent locates the journal even on a USB stick
  * larger than the original image. Only this adapter's virtual device shrinks;
  * no filesystem is allowed to write the unused physical tail. Legacy explicit
  * footer-only images remain readable at their original exact geometry. */
 u8 mbr[512];int r=raw_read((void*)h,0,mbr);if(r)return fail(r>0?IO:r);
 u32 limit=h->block.sectors,found=0;
 if(mbr[510]==0x55&&mbr[511]==0xaa)for(u32 i=0;i<4;i++){
  const u8*e=mbr+446+i*16;if(e[4]!=0xda)continue;
  u32 start=get32(e+8),n=get32(e+12);
  if(++found>1||(n<6||n>4+2*MULTIOS_UNDO_MAX_RECORDS||(n&1))||start<2048||start>=h->block.sectors||n>h->block.sectors-start)return fail(BAD);
  limit=start+n;
 }
 MultiBlock64Provider p={.abi=MULTIOS_BLOCK64_ABI,.sector_bytes=512,.sectors=limit,.ctx=(void*)h,.writable=h->block.writable,.read=raw_read,.write=raw_write,.flush=raw_flush};
 r=multi_block64_bind(&device,&p);if(r<0)return fail(r);
 u8 a[512],b[512];r=multi_block64_read(&device,limit-2,a);if(r<0)return fail(r);
 r=multi_block64_read(&device,limit-1,b);if(r<0)return fail(r);
 MultiUndoConfig c={0};r=locator(a,b,&c);if(r==NOTFOUND){state=1;return 0;}if(r<0)return fail(r);
 r=multi_undo_open(&journal,&device,&c);if(r<0)return fail(r);
 if(journal.active){r=multi_undo_recover(&journal);if(r<0)return fail(r);}
 state=2;return 0;
}
int multi_storage_matches(u32 base,u32 sectors){
 return state==2&&!multi_storage_blocked()&&journal.ready&&!journal.active&&
  journal.config.data_start==base&&journal.config.data_sectors==sectors;
}
int multi_storage_begin(void){
 if(!multi_storage_matches(vol.base,vol.sectors))return READONLY;
 int r=multi_undo_begin(&journal);if(r<0&&multi_storage_blocked()){state=-1;unmount_failed();}return r;
}
int multi_storage_reject(int r){
 if(r>=0)r=r?IO:INVALID;
 return state==2?multi_undo_abort(&journal,r):r;
}
int multi_storage_read(u32 lba,u8*b){
 if(multi_storage_blocked())return READONLY;
 int r=device.bound?multi_block64_read(&device,lba,b):raw_read((void*)multi_hal(),lba,b);
 return r?multi_storage_reject(r):0;
}
int multi_storage_flush(void){
 if(multi_storage_blocked())return READONLY;
 return device.bound?multi_block64_flush(&device):raw_flush((void*)multi_hal());
}
int multi_storage_write(u32 lba,const u8*b){
 if(state!=2||multi_storage_blocked()||!journal.active)return READONLY;
 return multi_undo_write(&journal,lba,b);
}
int multi_storage_finish(int r){
 if(state!=2||!journal.active){unmount_failed();return fail(BAD);}
 /* Mutators return exactly zero, never byte counts or positive errors. */
 if(r>0)r=IO;
 if(!r){r=multi_undo_commit(&journal);if(!r&&!multi_storage_blocked())return 0;if(!r)r=READONLY;}
 /* Never reuse FS allocation/directory caches after any unsuccessful mutation. */
 int original=r;
 if(!multi_storage_blocked()){
  int rollback=multi_undo_recover(&journal);if(rollback<0){state=-1;original=rollback;}
 }else state=-1;
 unmount_failed();return original;
}
