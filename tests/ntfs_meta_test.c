/* vm2 studios | t.me/v2vproj — independent vectors + randomized codec properties. */
#include "ntfs_meta.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t state=0x13004e54u;
static uint32_t rnd(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static void p16(u8*b,u16 n){b[0]=(u8)n;b[1]=(u8)(n>>8);}
static u16 g16(const u8*b){return b[0]|((u16)b[1]<<8);}
static u8 buf[65536],copy[65536],disk[65536];
static void usa(void){
 for(u32 sector=512;sector<=4096;sector*=2)for(u32 n=sector;n<=65536;n*=2){
  for(u32 i=0;i<n;i++)buf[i]=(u8)rnd();
  p16(buf+4,48);p16(buf+6,(u16)(n/sector+1));p16(buf+48,0xfffe);
  memcpy(copy,buf,n);assert(!ntfs_usa_encode(buf,n,sector));assert(g16(buf+48)==1);
  memcpy(disk,buf,n);
  for(u32 i=1;i<=n/sector;i++)assert(g16(buf+i*sector-2)==1);
  assert(!ntfs_usa_decode(buf,n,sector));
  /* USA array is intentionally replaced; every other original byte is restored. */
  u32 end=48+(n/sector+1)*2;
  for(u32 i=0;i<n;i++)if(i<48||i>=end)assert(buf[i]==copy[i]);
  /* Each damaged sector must fail WITHOUT partial restoration. */
  for(u32 i=1;i<=n/sector;i++){
   memcpy(buf,disk,n);buf[i*sector-2]^=1;memcpy(copy,buf,n);
   assert(ntfs_usa_decode(buf,n,sector)==BAD&&!memcmp(buf,copy,n));
  }
 }
 memset(buf,0,4096);p16(buf+4,48);p16(buf+6,3);p16(buf+48,2);p16(buf+510,2);p16(buf+1022,2);
 assert(!ntfs_usa_decode(buf,1024,512));
 const u32 sectors[]={0,511,513,8192};
 for(u32 j=0;j<4;j++){memcpy(copy,buf,4096);assert(ntfs_usa_encode(buf,1024,sectors[j])==BAD&&!memcmp(copy,buf,4096));}
 for(u32 off=0;off<520;off++){
  memset(buf,0,1024);p16(buf+4,(u16)off);p16(buf+6,3);memcpy(copy,buf,1024);
  int r=ntfs_usa_encode(buf,1024,512);
  if(off<8||off%2||off+6>510)assert(r==BAD&&!memcmp(copy,buf,1024));else assert(!r);
 }
 assert(ntfs_usa_decode(0,1024,512)==INVALID);
}
static void vector_tests(void){
 MultiNtfsRun out[8],before[8];memset(before,0x5a,sizeof(before));
 const u8 mapping[]={0x11,3,10,0x11,2,0xfb,0x01,4,0x11,1,2,0};
 u32 n=99;uint64_t end=999;assert(!ntfs_runs_decode(mapping,sizeof(mapping),7,100,out,8,&n,&end));
 assert(n==4&&end==17);
 assert(out[0].vcn==7&&out[0].lcn==10&&out[0].length==3&&!out[0].sparse);
 assert(out[1].vcn==10&&out[1].lcn==5&&out[1].length==2);
 assert(out[2].vcn==12&&out[2].sparse&&out[2].length==4);
 assert(out[3].vcn==16&&out[3].lcn==7&&out[3].length==1);
 u8 enc[128];u32 written;assert(!ntfs_runs_encode(out,n,100,enc,sizeof(enc),&written));
 assert(written==sizeof(mapping)&&!memcmp(mapping,enc,written));
 /* Sparse logical span may exceed all physical clusters. */
 const u8 huge[]={0x05,0,0,0,0,1,0};
 assert(!ntfs_runs_decode(huge,sizeof(huge),0,1,out,8,&n,&end));assert(end==UINT64_C(0x100000000)&&out[0].sparse);
 /* Positive five-byte LCN: never truncate to 32 bits. */
 const u8 large[]={0x51,1,0,0,0,0,1,0};
 assert(!ntfs_runs_decode(large,sizeof(large),0,UINT64_C(0x200000000),out,8,&n,&end));assert(out[0].lcn==UINT64_C(0x100000000));
 const u8 invalid[][12]={{0},{0x10,1,0},{0x19,1,1,0},{0x91,1,1,0},{0x11,0,1,0},{0x11,1,0xff,0},{0x11,2,99,0},{0x11,1,1,0x11,1,0x80,0}};
 for(u32 i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++){
  memcpy(out,before,sizeof(out));n=99;end=999;
  assert(ntfs_runs_decode(invalid[i],sizeof(invalid[i]),0,100,out,8,&n,&end)<0);
  assert(!memcmp(out,before,sizeof(out))&&n==99&&end==999);
 }
 for(u32 length=1;length<sizeof(mapping);length++){
  memcpy(out,before,sizeof(out));n=99;end=999;
  assert(ntfs_runs_decode(mapping,length,0,100,out,8,&n,&end)<0);
  assert(!memcmp(out,before,sizeof(out))&&n==99&&end==999);
 }
 memcpy(out,before,sizeof(out));assert(ntfs_runs_decode(mapping,sizeof(mapping),0,100,out,2,&n,&end)==NOSPACE&&!memcmp(out,before,sizeof(out)));
 const u8 overflow[]={1,2,0};assert(ntfs_runs_decode(overflow,3,UINT64_MAX-1,100,out,8,&n,&end)==BAD);
 MultiNtfsRun min_delta[]={{0,UINT64_C(0x7fffffffffffffff),1,0},{1,UINT64_C(0x8000000000000000),1,0},{2,0,1,0}};
 assert(!ntfs_runs_encode(min_delta,3,UINT64_MAX,enc,sizeof(enc),&written));
 assert(!ntfs_runs_decode(enc,written,0,UINT64_MAX,out,8,&n,&end));assert(n==3&&out[2].lcn==0);
 MultiNtfsRun impossible={0,UINT64_C(0x8000000000000000),1,0};
 memset(enc,0x55,sizeof(enc));written=88;assert(ntfs_runs_encode(&impossible,1,UINT64_MAX,enc,sizeof(enc),&written)==UNSUP&&written==88);
 for(u32 i=0;i<sizeof(enc);i++)assert(enc[i]==0x55);
}
static void properties(void){
 for(u32 trial=0;trial<10000;trial++){
  MultiNtfsRun in[16],out[16];u32 count=1+rnd()%16;uint64_t vcn=(uint64_t)rnd()<<16;const uint64_t clusters=UINT64_C(0x10000000000);
  for(u32 i=0;i<count;i++){
   uint64_t n=1+rnd()%65536, lcn=((uint64_t)(rnd()&127)<<32)|rnd();
   in[i]=(MultiNtfsRun){vcn,lcn,n,(rnd()%4)==0};vcn+=n;
  }
  u8 encoded[512],encoded2[512];memset(encoded,0xcc,sizeof(encoded));u32 bytes,bytes2,n;uint64_t end;
  assert(!ntfs_runs_encode(in,count,clusters,encoded,sizeof(encoded),&bytes));
  assert(!ntfs_runs_decode(encoded,bytes,in[0].vcn,clusters,out,16,&n,&end)&&n==count&&end==vcn);
  for(u32 i=0;i<n;i++)assert(out[i].vcn==in[i].vcn&&out[i].length==in[i].length&&out[i].sparse==in[i].sparse&&(in[i].sparse||out[i].lcn==in[i].lcn));
  assert(!ntfs_runs_encode(out,n,clusters,encoded2,sizeof(encoded2),&bytes2)&&bytes==bytes2&&!memcmp(encoded,encoded2,bytes));
  memset(encoded2,0x77,sizeof(encoded2));bytes2=99;
  assert(ntfs_runs_encode(in,count,clusters,encoded2,bytes-1,&bytes2)==NOSPACE&&bytes2==99);
  for(u32 i=0;i<sizeof(encoded2);i++)assert(encoded2[i]==0x77);
 }
 /* Bounded hostile byte strings: valid results must roundtrip, invalid outputs untouched. */
 for(u32 trial=0;trial<50000;trial++){
  u8 data[64],encoded[256];u32 length=1+rnd()%64;
  for(u32 i=0;i<length;i++)data[i]=(u8)rnd();
  MultiNtfsRun out[16],before[16];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));u32 n=55,written;uint64_t end=66;
  int r=ntfs_runs_decode(data,length,0,UINT64_C(0x1000000000000),out,16,&n,&end);
  if(r<0)assert(n==55&&end==66&&!memcmp(out,before,sizeof(out)));
  else assert(n&&n<=16&&end&& !ntfs_runs_encode(out,n,UINT64_C(0x1000000000000),encoded,sizeof(encoded),&written));
 }
}
int main(void){usa();vector_tests();properties();puts("PASS NTFS metadata: independent mapping-pair vectors; 10000 roundtrip properties; 50000 hostile inputs; USA 512/1024/2048/4096 sectors, all trailer failures atomic; encoding bounds/overflow/negative and 64-bit deltas/sparse runs. No disk writes or journal replay implemented.");}
