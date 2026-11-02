/* vm2 studios | t.me/v2vproj — synthetic providers, no device writes. */
#include "multios/block64.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u32 bytes=512,reads,writes,flushes;static uint64_t last;static int failure;
static int rd(void*c,uint64_t lba,u8*b){(void)c;last=lba;reads++;if(failure)return failure;for(u32 i=0;i<bytes;i++)b[i]=(u8)(lba+i);return 0;}
static int wr(void*c,uint64_t lba,const u8*b){(void)c;(void)b;last=lba;writes++;return failure;}
static int fl(void*c){(void)c;flushes++;return failure;}
static u8 buf[8192],scratch[4096];
static MultiBlock64Provider provider(void){MultiBlock64Provider p={MULTIOS_BLOCK64_ABI,bytes,UINT64_C(0x1000000000),0,1,rd,wr,fl};return p;}
int main(void){
 for(bytes=512;bytes<=4096;bytes*=2){
  MultiBlock64 io={0};MultiBlock64Provider p=provider();assert(!multi_block64_bind(&io,&p));
  uint64_t lba=UINT64_C(0x100000003);assert(!multi_block64_read(&io,lba,buf)&&last==lba);
  assert(!multi_block64_write(&io,lba,buf)&&last==lba);assert(multi_block64_bind(&io,&p)==INVALID);
  uint64_t offset=lba*bytes+bytes-3;u32 n=bytes+7;
  assert(!multi_block64_read_bytes(&io,offset,buf,n,scratch,sizeof(scratch)));
  for(u32 j=0;j<n;j++){uint64_t at=offset+j;assert(buf[j]==(u8)(at/bytes+at%bytes));}
  u32 before=reads;assert(multi_block64_read(&io,p.sectors,buf)==BAD&&reads==before);
  assert(multi_block64_read_bytes(&io,UINT64_MAX-3,buf,8,scratch,sizeof(scratch))==BAD&&reads==before);
  assert(multi_block64_read_bytes(&io,p.sectors*bytes-2,buf,3,scratch,sizeof(scratch))==BAD&&reads==before);
  assert(multi_block64_read_bytes(&io,0,buf,1,scratch,bytes-1)==INVALID);
  assert(!multi_block64_read_bytes(&io,UINT64_MAX,0,0,0,0));
  failure=1;assert(multi_block64_read(&io,0,buf)==IO&&io.failed&&!io.provider.writable);failure=0;
  before=writes;assert(multi_block64_write(&io,0,buf)==READONLY&&writes==before);
  before=flushes;assert(multi_block64_flush(&io)==READONLY&&flushes==before);
  assert(!multi_block64_read(&io,0,buf));
 }
 bytes=512;
 for(int mode=0;mode<3;mode++){
  MultiBlock64 io={0};MultiBlock64Provider p=provider();assert(!multi_block64_bind(&io,&p));failure=IO;
  int r=mode==0?multi_block64_read(&io,0,buf):mode==1?multi_block64_write(&io,0,buf):multi_block64_flush(&io);
  assert(r==IO&&io.failed&&!io.provider.writable);failure=0;
  assert(multi_block64_write(&io,0,buf)==READONLY);
  u32 before=flushes;assert(multi_block64_flush(&io)==READONLY&&flushes==before);
 }
 MultiBlock64 io={0};MultiBlock64Provider p=provider();p.writable=0;p.write=0;p.flush=0;
 assert(!multi_block64_bind(&io,&p)&&!multi_block64_flush(&io)&&multi_block64_write(&io,0,buf)==READONLY);
 MultiBlock64 invalid={0};p=provider();p.abi++;assert(multi_block64_bind(&invalid,&p)==INVALID&&!invalid.bound);
 p=provider();p.sector_bytes=513;assert(multi_block64_bind(&invalid,&p)==INVALID);
 p=provider();p.sectors=0;assert(multi_block64_bind(&invalid,&p)==INVALID);
 p=provider();p.flush=0;assert(multi_block64_bind(&invalid,&p)==INVALID);
 p=provider();p.read=0;assert(multi_block64_bind(&invalid,&p)==INVALID);
 /* Read the final representable byte without offset addition wrapping. */
 MultiBlock64 edge={0};p=provider();p.sectors=UINT64_MAX;p.writable=0;assert(!multi_block64_bind(&edge,&p));
 assert(!multi_block64_read_bytes(&edge,UINT64_MAX,buf,1,scratch,sizeof(scratch)));
 puts("PASS block64: exact LBA above 2^32; 512..4096 sectors; unaligned bounded reads; byte/LBA overflow; ABI validation; read/write/flush errors latch RO. Synthetic providers, not hardware certification.");
}
