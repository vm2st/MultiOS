/* vm2 studios | t.me/v2vproj — NTFS USA and mapping-pairs, no libc/OS services. */
#include "ntfs_meta.h"
#define U64MAX UINT64_MAX
static u16 le16(const u8 *p) { return (u16)p[0] | (u16)((u16)p[1] << 8); }
static void wr16(u8 *p, u16 n) { p[0]=(u8)n; p[1]=(u8)(n>>8); }
static int usa_shape(const u8 *b,u32 n,u32 sector,u32 *offset,u32 *count) {
    if(!b || !offset || !count) return INVALID;
    if(sector<512 || sector>4096 || (sector&(sector-1)) || n<sector || n>65536 || n%sector) return BAD;
    u32 off=le16(b+4),k=le16(b+6);
    /* USA must fit in the first sector without covering its protected trailer. */
    if(k!=n/sector+1 || off<8 || off%2 || off>sector-2 || k>(sector-2-off)/2) return BAD;
    *offset=off;*count=k;return 0;
}
int ntfs_usa_decode(u8 *b,u32 n,u32 sector) {
    u32 off,k;TRY(usa_shape(b,n,sector,&off,&k));
    u16 sequence=le16(b+off);
    if(!sequence || sequence==0xffff) return BAD;
    /* Validate EVERY trailer before restoring ANY bytes. */
    for(u32 i=1;i<k;i++) if(le16(b+i*sector-2)!=sequence) return BAD;
    for(u32 i=1;i<k;i++) wr16(b+i*sector-2,le16(b+off+i*2));
    return 0;
}
int ntfs_usa_encode(u8 *b,u32 n,u32 sector) {
    u32 off,k;TRY(usa_shape(b,n,sector,&off,&k));
    u16 sequence=(u16)(le16(b+off)+1);
    if(!sequence || sequence==0xffff) sequence=1;
    for(u32 i=1;i<k;i++) wr16(b+off+i*2,le16(b+i*sector-2));
    wr16(b+off,sequence);
    for(u32 i=1;i<k;i++) wr16(b+i*sector-2,sequence);
    return 0;
}
static uint64_t readle(const u8 *b,u32 n) {
    uint64_t v=0;for(u32 i=0;i<n;i++) v|=(uint64_t)b[i]<<(8*i);return v;
}
static int decode_pass(const u8 *b,u32 bytes,uint64_t first,uint64_t clusters,
                       MultiNtfsRun *out,u32 cap,u32 *number,uint64_t *next) {
    u32 pos=0,k=0;uint64_t vcn=first,lcn=0;
    while(pos<bytes) {
        u8 tag=b[pos++];
        if(!tag) { if(!k) return BAD;*number=k;*next=vcn;return 0; }
        u32 ls=tag&15,os=tag>>4;
        if(!ls || ls>8 || os>8 || ls+os>bytes-pos) return BAD;
        uint64_t n=readle(b+pos,ls);pos+=ls;
        if(!n || n>U64MAX-vcn) return BAD;
        if(k==cap) return NOSPACE;
        int hole=!os;
        if(os) {
            uint64_t d=readle(b+pos,os);
            if(b[pos+os-1]&128) {
                if(os<8) d|=U64MAX<<(os*8);
                uint64_t mag=(~d)+1;
                if(mag>lcn) return BAD;
                lcn-=mag;
            } else {
                if(d>U64MAX-lcn) return BAD;
                lcn+=d;
            }
            if(lcn>=clusters || n>clusters-lcn) return BAD;
        }
        pos+=os;
        if(out) out[k]=(MultiNtfsRun){vcn,hole?0:lcn,n,hole};
        k++;vcn+=n;
    }
    return BAD;
}
int ntfs_runs_decode(const u8 *b,u32 n,uint64_t first,uint64_t clusters,
                     MultiNtfsRun *out,u32 cap,u32 *count,uint64_t *next) {
    if(!b || !out || !count || !next || !n || !clusters) return INVALID;
    u32 k;uint64_t end;
    TRY(decode_pass(b,n,first,clusters,0,cap,&k,&end));
    TRY(decode_pass(b,n,first,clusters,out,cap,&k,&end));
    *count=k;*next=end;return 0;
}
static u32 unsigned_width(uint64_t v) {
    u32 n=1;while(n<8 && (v>>(n*8))) n++;return n;
}
static u32 delta_width(uint64_t mag,int negative) {
    for(u32 n=1;n<8;n++) {
        uint64_t sign=(uint64_t)1<<(n*8-1);
        if(negative ? mag<=sign : mag<sign) return n;
    }
    return 8;
}
static int encode_pass(const MultiNtfsRun *r,u32 count,uint64_t clusters,
                       u8 *out,u32 cap,u32 *written) {
    uint64_t lcn=0,next=r[0].vcn;u32 pos=0;
    for(u32 i=0;i<count;i++) {
        uint64_t n=r[i].length;
        if(!n || r[i].vcn!=next || n>U64MAX-next || (r[i].sparse!=0 && r[i].sparse!=1)) return BAD;
        u32 ls=unsigned_width(n),os=0;uint64_t delta=0;
        if(!r[i].sparse) {
            uint64_t target=r[i].lcn;
            if(target>=clusters || n>clusters-target) return BAD;
            int neg=target<lcn;uint64_t mag=neg?lcn-target:target-lcn;
            if(neg ? mag>((uint64_t)1<<63) : mag>(((uint64_t)1<<63)-1)) return UNSUP;
            os=delta_width(mag,neg);delta=neg?(~mag)+1:mag;lcn=target;
        }
        u32 need=1+ls+os;
        if(pos>=cap || need>=cap-pos) return NOSPACE; /* keep one byte for terminator */
        if(out) {
            out[pos]=(u8)(ls|(os<<4));
            for(u32 j=0;j<ls;j++) out[pos+1+j]=(u8)(n>>(j*8));
            for(u32 j=0;j<os;j++) out[pos+1+ls+j]=(u8)(delta>>(j*8));
        }
        pos+=need;next+=n;
    }
    if(pos>=cap) return NOSPACE;
    if(out) out[pos]=0;
    *written=pos+1;return 0;
}
int ntfs_runs_encode(const MultiNtfsRun *r,u32 count,uint64_t clusters,
                     u8 *out,u32 cap,u32 *written) {
    if(!r || !count || !clusters || !out || !written) return INVALID;
    u32 n;TRY(encode_pass(r,count,clusters,0,cap,&n));
    TRY(encode_pass(r,count,clusters,out,cap,&n));*written=n;return 0;
}
