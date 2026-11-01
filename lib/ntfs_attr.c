/* vm2 studios | t.me/v2vproj — checked FILE attributes and $ATTRIBUTE_LIST entries. */
#include "ntfs_attr.h"
static u16 r16(const u8*p){return (u16)p[0]|((u16)p[1]<<8);}
static u32 r32(const u8*p){return (u32)r16(p)|((u32)r16(p+2)<<16);}
static uint64_t r64(const u8*p){return (uint64_t)r32(p)|((uint64_t)r32(p+4)<<32);}
static int name_ok(u32 units,u32 off,u32 header,u32 end){
    return !units || (off>=header && !(off&1) && off<=end && units*2<=end-off);
}
int ntfs_attr_next(const u8 *b,u32 bytes,u32 *cursor,MultiNtfsAttr *out){
    if(!b||!cursor||!out)return INVALID;
    if(bytes<48||b[0]!='F'||b[1]!='I'||b[2]!='L'||b[3]!='E')return BAD;
    u32 used=r32(b+24),first=r16(b+20),p=*cursor;
    if(used>bytes||used<48||first<48||(first&7)||p<first||(p&7)||p>used||used-p<4)return BAD;
    const u8 *a=b+p;u32 type=r32(a);
    if(type==UINT32_MAX)return 0;
    if(!type || used-p<16)return BAD;
    u32 n=r32(a+4);u8 nr=a[8],units=a[9];u32 noff=r16(a+10);
    if(n<24||(n&7)||n>used-p||nr>1)return BAD;
    MultiNtfsAttr v={0};v.raw=a;v.type=type;v.bytes=n;v.nonresident=nr;
    v.id=r16(a+14);v.flags=r16(a+12);v.name_units=units;
    if(!nr){
        u32 size=r32(a+16),off=r16(a+20);
        if(off<24||off>n||size>n-off||!name_ok(units,noff,24,off))return BAD;
        v.value=a+off;v.value_bytes=size;v.size=size;v.initialized=size;
    }else{
        u32 header=(v.flags&0x8001)?72:64;
        if(n<header)return BAD;
        u32 off=r16(a+32);
        if(off<header||off>=n||!name_ok(units,noff,header,off))return BAD;
        v.first_vcn=r64(a+16);v.last_vcn=r64(a+24);
        if(v.last_vcn<v.first_vcn && !(v.first_vcn==0&&v.last_vcn==UINT64_MAX))return BAD;
        v.compression_unit=r16(a+34);v.mapping=a+off;v.mapping_bytes=n-off;
        v.allocated=r64(a+40);v.size=r64(a+48);v.initialized=r64(a+56);
        if(!v.first_vcn && v.initialized>v.size)return BAD;
    }
    if(units)v.name=a+noff;
    *out=v;*cursor=p+n;return 1;
}
int ntfs_attrlist_next(const u8 *b,u32 bytes,u32 *cursor,MultiNtfsAttrRef *out){
    if(!b||!cursor||!out)return INVALID;
    u32 p=*cursor;
    if(p==bytes)return 0;
    if(p>bytes || bytes-p<26 || (p&7))return BAD;
    const u8 *a=b+p;u32 n=r16(a+4),units=a[6],off=a[7],type=r32(a);
    if(!type||type==UINT32_MAX||n<26||(n&7)||n>bytes-p||!name_ok(units,off,26,n))return BAD;
    MultiNtfsAttrRef v={0};v.type=type;v.bytes=n;v.name_units=(u8)units;v.id=r16(a+24);
    v.first_vcn=r64(a+8);uint64_t ref=r64(a+16);
    v.record=ref&UINT64_C(0x0000ffffffffffff);v.sequence=(u16)(ref>>48);
    if(units)v.name=a+off;
    *out=v;*cursor=p+n;return 1;
}
