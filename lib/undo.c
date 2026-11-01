/* MultiPulti | vm2 studios | t.me/v2vproj — private bounded undo protocol v1.
 * Experimental storage building block only. No filesystem is made RW here.
 */
#include "multios/undo.h"
#define CLEAN 0u
#define ACTIVE 1u
static const u8 header_magic[8]={'M','P','U','N','D','O','1',0};
static const u8 record_magic[8]={'M','P','U','N','D','O','R','1'};
static u32 get32(const u8 *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static uint64_t get64(const u8 *p){return get32(p)|((uint64_t)get32(p+4)<<32);}
static void put32(u8 *p,u32 v){for(u32 i=0;i<4;i++){p[i]=(u8)v;v>>=8;}}
static void put64(u8 *p,uint64_t v){put32(p,(u32)v);put32(p+4,(u32)(v>>32));}
static int equal(const u8 *a,const u8 *b,u32 n){for(u32 i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static u32 crc(const u8 *p,u32 n){u32 c=UINT32_MAX;while(n--){c^=*p++;for(u32 i=0;i<8;i++)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
static int zero(const u8 *p,u32 n){while(n--)if(*p++)return 0;return 1;}
static int config_ok(MultiBlock64 *io,const MultiUndoConfig *c){
 if(!io||!io->bound||!c||io->provider.sector_bytes!=512)return INVALID;
 if((c->journal_sectors&1)||c->journal_sectors<4||c->journal_sectors>2+2*MULTIOS_UNDO_MAX_RECORDS||
    !c->data_sectors||c->journal_start>=io->provider.sectors||c->data_start>=io->provider.sectors||
    c->journal_sectors>io->provider.sectors-c->journal_start||
    c->data_sectors>io->provider.sectors-c->data_start||zero(c->media_id,16))return INVALID;
 /* Ends cannot wrap after the subtraction checks above. */
 if(c->journal_start<c->data_start+c->data_sectors&&c->data_start<c->journal_start+c->journal_sectors)return INVALID;
 return 0;
}
static int writable(MultiBlock64 *io){return !io->failed&&io->provider.writable&&io->provider.write&&io->provider.flush;}
static int status(MultiUndo *j,int r){if(r<0)j->failed=1;return r;}
static int state_ok(MultiUndo *j){
 if(!j||!j->ready||!j->io)return INVALID;
 if(j->failed||j->io->failed)return READONLY;
 return 0;
}
static int target_ok(const MultiUndo *j,uint64_t lba){return lba>=j->config.data_start&&lba-j->config.data_start<j->config.data_sectors;}
static void make_header(const MultiUndo *j,u32 active,u32 count,uint64_t seq,uint64_t tx,u8 *b){
 mzero(b,512);mcopy(b,header_magic,8);put32(b+8,1);put32(b+12,512);put32(b+16,512);
 put32(b+20,active);put64(b+24,seq);put64(b+32,tx);put32(b+40,count);put32(b+44,j->capacity);
 put64(b+48,j->config.journal_start);put64(b+56,j->config.journal_sectors);
 put64(b+64,j->config.data_start);put64(b+72,j->config.data_sectors);put64(b+80,j->io->provider.sectors);
 mcopy(b+88,j->config.media_id,16);put32(b+508,crc(b,508));
}
/* Both mirrors must be durable BEFORE the next target write. With honest flush,
 * a torn mirror can never erase the only durable description of a target write.
 * On commit, a durable first CLEAN mirror is a possible commit point even when
 * the second fails: the caller must treat the outcome as unknown and reopen.
 */
static int save(MultiUndo *j,u32 active,u32 count,uint64_t tx){
 if(j->sequence==UINT64_MAX)return UNSUP;
 u8 b[512];uint64_t next=j->sequence+1;make_header(j,active,count,next,tx,b);
 /* Preserve the newest/sole valid mirror until the older/invalid one is
  * durably replaced. Reopening a previously interrupted save can reverse the
  * slot order. Always writing slot 0 first permits generation gaps after a
  * SECOND interruption and can destroy the sole usable header. */
 u32 first=j->mirror_first;
 if(first>1)return status(j,BAD);
 for(u32 i=0;i<2;i++){u32 slot=i?(first^1u):first;
  int r=multi_block64_write(j->io,j->config.journal_start+slot,b);if(r<0)return status(j,r);
  r=multi_block64_flush(j->io);if(r<0)return status(j,r);
  u8 check[512];r=multi_block64_read(j->io,j->config.journal_start+slot,check);
  if(r<0)return status(j,r);if(!equal(check,b,512))return status(j,IO);
 }
 j->sequence=next;j->transaction=tx;j->count=count;j->active=(int)active;j->mirror_first=0;j->degraded=0;if(!active)j->owned=0;return 0;
}
int multi_undo_format(MultiBlock64 *io,const MultiUndoConfig *c){
 TRY(config_ok(io,c));if(!writable(io))return READONLY;u8 b[512];
 for(uint64_t i=0;i<c->journal_sectors;i++){TRY(multi_block64_read(io,c->journal_start+i,b));if(!zero(b,512))return EXISTS;}
 MultiUndo j={0};j.io=io;j.config=*c;j.capacity=(u32)((c->journal_sectors-2)>>1);
 return save(&j,CLEAN,0,0);
}
/* 1 valid for this exact binding; 0 torn/unrecognized; negative wrong binding.
 * A CRC-valid foreign header is NEVER ignored in favour of another mirror.
 */
static int header_ok(const MultiUndo *j,const u8 *b){
 if(!equal(b,header_magic,8)||get32(b+508)!=crc(b,508))return 0;
 if(get32(b+8)!=1||get32(b+12)!=512||get32(b+16)!=512||!zero(b+104,404))return BAD;
 if(get64(b+48)!=j->config.journal_start||get64(b+56)!=j->config.journal_sectors||
    get64(b+64)!=j->config.data_start||get64(b+72)!=j->config.data_sectors||
    get64(b+80)!=j->io->provider.sectors||!equal(b+88,j->config.media_id,16)||
    get32(b+44)!=j->capacity)return BAD;
 u32 state=get32(b+20),count=get32(b+40);uint64_t seq=get64(b+24),tx=get64(b+32);
 if(!seq||state>1||count>j->capacity||(!state&&count)||tx>seq||(state&&!tx))return BAD;
 return 1;
}
/* Adjacent durable mirrors can only be produced by one protocol step.
 * Valid CRC alone is insufficient to accept a foreign/impossible transition. */
static int transition_ok(const u8*low,const u8*high){
 u32 a=get32(low+20),b=get32(high+20),n=get32(low+40),m=get32(high+40);
 uint64_t tx=get64(high+32);
 if(!a)return b==ACTIVE&&!m&&tx==get64(high+24);
 if(tx!=get64(low+32))return 0;
 return b==CLEAN ? !m : m==n+1;
}
static void make_record(MultiUndo *j,u32 index,uint64_t lba,u32 data_crc,u8 *b){
 mzero(b,512);mcopy(b,record_magic,8);put32(b+8,1);put32(b+12,index);put64(b+16,j->transaction);
 put64(b+24,lba);put32(b+32,data_crc);mcopy(b+40,j->config.media_id,16);put32(b+508,crc(b,508));
}
static int record(MultiUndo *j,u32 index,u8 *data,uint64_t *lba){
 u8 b[512];uint64_t sector=j->config.journal_start+2+((uint64_t)index<<1);
 TRY(multi_block64_read(j->io,sector,b));
 if(!equal(b,record_magic,8)||get32(b+508)!=crc(b,508)||get32(b+8)!=1||get32(b+12)!=index||
    get64(b+16)!=j->transaction||!equal(b+40,j->config.media_id,16)||!zero(b+36,4)||!zero(b+56,452))return BAD;
 uint64_t target=get64(b+24);if(!target_ok(j,target))return BAD;
 TRY(multi_block64_read(j->io,sector+1,data));if(get32(b+32)!=crc(data,512))return BAD;
 *lba=target;return 0;
}
static int validate_records(MultiUndo *j){
 u8 data[512];for(u32 i=0;i<j->count;i++){
  uint64_t lba;TRY(record(j,i,data,&lba));
  for(u32 k=0;k<i;k++)if(j->targets[k]==lba)return BAD;
  j->targets[i]=lba;
 }
 return 0;
}
int multi_undo_open(MultiUndo *j,MultiBlock64 *io,const MultiUndoConfig *c){
 if(!j||j->ready)return INVALID;TRY(config_ok(io,c));MultiUndoConfig saved=*c;
 mzero(j,sizeof(*j));j->io=io;j->config=saved;j->capacity=(u32)((saved.journal_sectors-2)>>1);
 u8 a[512],b[512];int r=multi_block64_read(io,saved.journal_start,a);if(r<0)return status(j,r);
 r=multi_block64_read(io,saved.journal_start+1,b);if(r<0)return status(j,r);
 int x=header_ok(j,a),y=header_ok(j,b);if(x<0||y<0)return status(j,BAD);
 if(!x&&!y)return status(j,BAD);
 const u8 *h=x?a:b;
 j->degraded=!(x&&y);
 /* If one copy is invalid, replace that slot first on the next mutation.
  * Opening itself stays read-only; never repair an unmounted unknown medium. */
 j->mirror_first=x?1u:0u;
 if(x&&y){uint64_t s=get64(a+24),t=get64(b+24);
  if(s==t){if(!equal(a,b,512))return status(j,BAD);j->mirror_first=0;}
  else {uint64_t high=s>t?s:t,low=s>t?t:s;if(high-low!=1||!transition_ok(s>t?b:a,s>t?a:b))return status(j,BAD);h=s>t?a:b;j->mirror_first=s>t?1u:0u;j->degraded=1;}
 }
 j->sequence=get64(h+24);j->transaction=get64(h+32);j->count=get32(h+40);j->active=(int)get32(h+20);
 r=validate_records(j);if(r<0)return status(j,r);j->ready=1;return 0;
}
int multi_undo_begin(MultiUndo *j){
 TRY(state_ok(j));if(j->active)return INVALID;if(!writable(j->io))return READONLY;
 /* Reserve begin, every unique-sector record and the final CLEAN header. */
 if(j->sequence>UINT64_MAX-((uint64_t)j->capacity+2))return UNSUP;
 int r=save(j,ACTIVE,0,j->sequence+1);if(!r)j->owned=1;return r;
}
int multi_undo_read(MultiUndo *j,uint64_t lba,u8 *b){
 TRY(state_ok(j));if(!b)return INVALID;if(!target_ok(j,lba))return BAD;
 int r=multi_block64_read(j->io,lba,b);return r<0?status(j,r):r;
}
int multi_undo_write(MultiUndo *j,uint64_t lba,const u8 *b){
 TRY(state_ok(j));if(!b)return INVALID;if(!j->active)return INVALID;
 if(!target_ok(j,lba))return BAD;if(!writable(j->io))return READONLY;
 if(j->sequence==UINT64_MAX)return UNSUP;
 if(!j->owned)return INVALID;
 u32 index=0;while(index<j->count&&j->targets[index]!=lba)index++;
 if(index==j->count){
  if(index==j->capacity)return NOSPACE;
  u8 original[512],desc[512];int r=multi_block64_read(j->io,lba,original);if(r<0)return status(j,r);
  make_record(j,index,lba,crc(original,512),desc);
  uint64_t sector=j->config.journal_start+2+((uint64_t)index<<1);
  r=multi_block64_write(j->io,sector+1,original);if(r<0)return status(j,r);
  r=multi_block64_write(j->io,sector,desc);if(r<0)return status(j,r);
  r=multi_block64_flush(j->io);if(r<0)return status(j,r);
  /* Read back both durable before-image and descriptor before publishing the
   * record count. A success-returning dropped/corrupt write must not expose a
   * target sector whose rollback data is missing. */
  u8 check[512];r=multi_block64_read(j->io,sector+1,check);if(r<0)return status(j,r);
  if(!equal(check,original,512))return status(j,IO);
  r=multi_block64_read(j->io,sector,check);if(r<0)return status(j,r);
  if(!equal(check,desc,512))return status(j,IO);
  TRY(save(j,ACTIVE,index+1,j->transaction));j->targets[index]=lba;
 }
 int r=multi_block64_write(j->io,lba,b);if(r<0)return status(j,r);j->expected_crc[index]=crc(b,512);return r;
}
int multi_undo_commit(MultiUndo *j){
 TRY(state_ok(j));if(!j->active)return INVALID;if(!writable(j->io))return READONLY;
 if(j->sequence==UINT64_MAX)return UNSUP;
 int r=validate_records(j);if(r<0)return status(j,r);
 if(!j->owned)return INVALID;
 r=multi_block64_flush(j->io);if(r<0)return status(j,r);
 /* Validate the last requested contents of EVERY unique target after flush,
  * before the first CLEAN mirror can become a commit point. CRC is accidental
  * corruption detection, not authentication nor proof against lying caches. */
 u8 check[512];for(u32 i=0;i<j->count;i++){
  r=multi_block64_read(j->io,j->targets[i],check);if(r<0)return status(j,r);
  if(crc(check,512)!=j->expected_crc[i])return status(j,IO);
 }
 return save(j,CLEAN,0,j->transaction);
}
int multi_undo_recover(MultiUndo *j){
 TRY(state_ok(j));if(!j->active)return 0;if(!writable(j->io))return READONLY;
 if(j->sequence==UINT64_MAX)return UNSUP;
 int r=validate_records(j);if(r<0)return status(j,r);
 u8 data[512];for(u32 n=j->count;n;n--){uint64_t lba;
  r=record(j,n-1,data,&lba);if(r<0)return status(j,r);
  r=multi_block64_write(j->io,lba,data);if(r<0)return status(j,r);
 }
 r=multi_block64_flush(j->io);if(r<0)return status(j,r);
 /* Never erase recovery intent after a silently dropped/torn restore. */
 for(u32 i=0;i<j->count;i++){uint64_t lba;u8 check[512];
  r=record(j,i,data,&lba);if(r<0)return status(j,r);
  r=multi_block64_read(j->io,lba,check);if(r<0)return status(j,r);
  if(!equal(check,data,512))return status(j,IO);
 }
 return save(j,CLEAN,0,j->transaction);
}
