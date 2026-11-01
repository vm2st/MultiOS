/* vm2 studios | t.me/v2vproj — bounded attribute parser regressions. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ntfs_attr.h"
static void w16(u8*b,u16 v){b[0]=v;b[1]=v>>8;}
static void w32(u8*b,u32 v){w16(b,v);w16(b+2,v>>16);}
static void w64(u8*b,uint64_t v){w32(b,v);w32(b+4,v>>32);}
static u32 seed=0x10203040;static u32 rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void rec(u8*b,int nr){memset(b,0,512);memcpy(b,"FILE",4);w16(b+20,56);w32(b+24,512);u8*a=b+56;w32(a,0x80);w32(a+4,nr?80:32);a[8]=nr;w16(a+14,7);if(nr){w16(a+32,64);w64(a+24,1);w64(a+40,8192);w64(a+48,5000);w64(a+56,4000);a[64]=0x11;a[65]=2;a[66]=10;}else{w32(a+16,3);w16(a+20,24);memcpy(a+24,"abc",3);}w32(a+(nr?80:32),UINT32_MAX);}
static void atomic_rec(u8*b,u32 bytes,u32 p){MultiNtfsAttr a,old;memset(&a,0xa5,sizeof(a));memcpy(&old,&a,sizeof(a));u32 before=p;int r=ntfs_attr_next(b,bytes,&p,&a);if(r<=0){assert(p==before&&!memcmp(&old,&a,sizeof(a)));}else{assert(p>before&&p<=bytes);assert(a.raw>=b&&a.raw+a.bytes<=b+bytes);}}
static void atomic_list(u8*b,u32 bytes,u32 p){MultiNtfsAttrRef a,old;memset(&a,0xa5,sizeof(a));memcpy(&old,&a,sizeof(a));u32 before=p;int r=ntfs_attrlist_next(b,bytes,&p,&a);if(r<=0){assert(p==before&&!memcmp(&old,&a,sizeof(a)));}else assert(p>before&&p<=bytes);}
int main(void){u8 b[512],orig[512];MultiNtfsAttr a;MultiNtfsAttrRef ref;u32 p;
 rec(b,0);p=56;assert(ntfs_attr_next(b,512,&p,&a)==1&&p==88&&a.id==7&&a.value_bytes==3&&!memcmp(a.value,"abc",3));assert(ntfs_attr_next(b,512,&p,&a)==0);atomic_rec(b,512,p);
 rec(b,1);p=56;assert(ntfs_attr_next(b,512,&p,&a)==1&&a.nonresident&&a.last_vcn==1&&a.size==5000&&a.initialized==4000);
 /* Named resident/nonresident attributes and UTF-16 views. */
 rec(b,0);w32(b+60,40);b[65]=2;w16(b+66,24);w16(b+76,28);w16(b+80,'X');w16(b+82,0x410);memcpy(b+84,"abc",3);w32(b+96,UINT32_MAX);p=56;assert(ntfs_attr_next(b,512,&p,&a)==1&&a.name_units==2&&a.name==b+80&&a.value==b+84);
 rec(b,1);b[65]=2;w16(b+66,64);w16(b+88,68);w16(b+120,'X');w16(b+122,'Y');b[124]=0x11;b[125]=2;b[126]=10;b[127]=0;p=56;assert(ntfs_attr_next(b,512,&p,&a)==1&&a.mapping==b+124);
 rec(b,1);w64(b+72,UINT64_C(0x100000000));w64(b+80,UINT64_C(0x100000005));p=56;assert(ntfs_attr_next(b,512,&p,&a)==1&&a.first_vcn==UINT64_C(0x100000000));
 rec(b,1);w16(b+68,0x8000);p=56;assert(ntfs_attr_next(b,512,&p,&a)<0);w16(b+88,72);b[128]=0;p=56;assert(ntfs_attr_next(b,512,&p,&a)==1);
 rec(b,1);memcpy(orig,b,512);
 unsigned positions[]={4,8,9,10,12,16,20,24,28,32,34,40,48,56};
 for(unsigned j=0;j<sizeof(positions)/sizeof(*positions);j++)for(unsigned k=0;k<8;k++){memcpy(b,orig,512);b[56+positions[j]]^=1u<<k;atomic_rec(b,512,56);}
 memset(b,0,512);w32(b,0x80);w16(b+4,32);w64(b+8,UINT64_C(0x100000000));w64(b+16,UINT64_C(0x1234abcd12345678));w16(b+24,99);p=0;assert(ntfs_attrlist_next(b,32,&p,&ref)==1&&p==32&&ref.record==UINT64_C(0xabcd12345678)&&ref.sequence==0x1234&&ref.first_vcn==UINT64_C(0x100000000)&&ref.id==99);assert(ntfs_attrlist_next(b,32,&p,&ref)==0);
 b[6]=2;b[7]=28;w16(b+28,'A');w16(b+30,0x411);p=0;assert(ntfs_attrlist_next(b,32,&p,&ref)==1&&ref.name==b+28);b[7]=25;p=0;assert(ntfs_attrlist_next(b,32,&p,&ref)<0);atomic_list(b,32,0);
 /* Mutated valid seeds + random buffers, always using the real allocation bound. */
 for(unsigned j=0;j<50000;j++){
  if(j&1){rec(b,j&2);for(unsigned k=0;k<1+(rng()%8);k++)b[rng()%512]^=(u8)rng();}else for(unsigned k=0;k<512;k++)b[k]=(u8)rng();
  u32 bytes=rng()%513;atomic_rec(b,bytes,(j%3)?56:rng());atomic_list(b,bytes,(j%3)?0:rng());
 }
 puts("PASS NTFS attributes: resident/nonresident, names, 64-bit refs/VCNs, atomic errors; 50000 hostile record/list inputs.");return 0;}
