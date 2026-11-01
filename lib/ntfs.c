/* vm2 studios | t.me/v2vproj — NTFS 3.x READ-ONLY backend.
 * Uses bounded MFT scanning, not the directory B+tree. No NTFS writes, journal
 * replay, full MFT bootstrap lists, compression or EFS. Never modifies the source disk.
 * 32-bit block ABI: <=128 GiB disk, 512-byte logical sectors; extended read API supports large streams.
 */
#include "fs_backend.h"
#include "ntfs_meta.h"
#include "ntfs_attr.h"
#include "unicode.h"
#include "multios/block64.h"
#define RECORD_MAX 4096u
#define RUN_MAX 32u
#define MFT_MAX 65536u
#define SCAN_MAX 65536u
static u8 record[RECORD_MAX];
static u32 record_bytes, mft_size, mft_valid, mft_records;
typedef struct {
    u32 vcn, lcn, count;
    int hole;
} Run;
static Run mft_runs[RUN_MAX];
static u32 mft_run_count;
static MultiBlock64 ntfs_io;
static Stream ntfs_upcase_stream;static u8 ntfs_case_cache[512];
static u32 ntfs_case_page;static int ntfs_case_valid;
/* Explicit compatibility adapter: no silent truncation into legacy 32-bit HAL. */
static int legacy_read(void *ctx,uint64_t lba,u8 *b) {
    (void)ctx;
    if(lba>UINT32_MAX) return UNSUP;
    return disk_read((u32)lba,b);
}
static u16 r16(const u8 *p) {
    return p[0] | ((u16)p[1] << 8);
}
static u32 r32(const u8 *p) {
    return r16(p) | ((u32)r16(p + 2) << 16);
}
static int eq4(const u8 *p, const char *n) {
    return p[0] == (u8)n[0] && p[1] == (u8)n[1] && p[2] == (u8)n[2] && p[3] == (u8)n[3];
}
static int rd(u32 sector, u8 *b) {
    if (sector >= vol.sectors)
        return BAD;
    return multi_block64_read(&ntfs_io,(uint64_t)vol.base+sector,b);
}
static int record_ok(void) {
    if (!eq4(record, "FILE"))
        return BAD;
    if(r16(record+4)<48) return BAD;
    TRY(ntfs_usa_decode(record, record_bytes, 512));
    u32 used = r32(record + 24), allocated = r32(record + 28), a = r16(record + 20);
    if (allocated != record_bytes || used > allocated || used < 48 || a < 48 || a % 8 ||
        a > used - 4 || (u32)r16(record+4)+(u32)r16(record+6)*2>a)
        return BAD;
    return 0;
}
/* The caller reads attribute values before another MFT record is loaded. */
static int attr_next(u32 *offset, const u8 **a) {
    MultiNtfsAttr item;
    int r=ntfs_attr_next(record,record_bytes,offset,&item);
    if(r>0)*a=item.raw;
    return r;
}
static int runs(const u8 *a, Run *out, u32 *count, u32 size, int mft) {
    if (!a[8] || r32(a + 16) || r32(a + 20) || r32(a + 28) ||
        (r16(a + 34) && (!(r16(a + 12) & 0x8000) || r16(a + 34) > 4)))
        return UNSUP;
    if (r16(a + 12) & 0x4001)
        return UNSUP; /* compression / EFS */
    u32 length = r32(a + 4), pos = r16(a + 32), k;
    if(pos>=length) return BAD;
    MultiNtfsRun decoded[RUN_MAX];uint64_t next;
    TRY(ntfs_runs_decode(a+pos,length-pos,0,vol.clusters,decoded,RUN_MAX,&k,&next));
    if(next>UINT32_MAX || r32(a+24)!=next-1) return BAD;
    if(size && 1+(size-1)/vol.cluster_bytes>next) return BAD;
    for(u32 i=0;i<k;i++) {
        if(decoded[i].vcn>UINT32_MAX || decoded[i].lcn>UINT32_MAX || decoded[i].length>UINT32_MAX) return UNSUP;
        if(decoded[i].sparse && mft) return UNSUP;
        out[i]=(Run){(u32)decoded[i].vcn,(u32)decoded[i].lcn,(u32)decoded[i].length,decoded[i].sparse};
    }
    *count=k;
    return 0;
}
static int runread(const Run *map, u32 count, uint64_t off, u8 *b, u32 n) {
    u32 shift=0;while((1u<<shift)<vol.cluster_bytes)shift++;
    while (n) {
        uint64_t wide_vcn=off>>shift;if(wide_vcn>UINT32_MAX)return UNSUP;
        u32 vcn = (u32)wide_vcn, index = 0;
        while (index < count && (vcn < map[index].vcn || vcn - map[index].vcn >= map[index].count))
            index++;
        if (index == count)
            return BAD;
        u32 part = off % 512, take = 512 - part;
        if (take > n)
            take = n;
        if (map[index].hole)
            mzero(b, take);
        else {
            u32 c = map[index].lcn + (vcn - map[index].vcn);
            u8 sector[512];
            TRY(rd(c * vol.spc + (((u32)off & (vol.cluster_bytes-1)) >> 9), sector));
            mcopy(b, sector + part, take);
        }
        b += take;
        off += take;
        n -= take;
    }
    return 0;
}
static int load(u32 number) {
    if (number >= mft_records || number > 0xffffffffu / record_bytes)
        return BAD;
    u32 off = number * record_bytes;
    if (off > mft_valid || record_bytes > mft_valid - off)
        return BAD;
    TRY(runread(mft_runs, mft_run_count, off, record, record_bytes));
    return record_ok();
}
static int data_attr(const u8 **data) {
    u32 off = r16(record + 20);
    const u8 *a;
    int r;
    *data = 0;
    while ((r = attr_next(&off, &a)) > 0) {
        u32 type = r32(a);
        if (type == 0x20 || type == 0xc0)
            return UNSUP; /* Only the initial $MFT bootstrap lacks ATTRIBUTE_LIST support. */
        if (type == 0x80 && !a[9]) {
            if (*data)
                return UNSUP;
            *data = a;
        }
    }
    return r;
}
#define LIST_MAX 16384u
static u8 attrlist[LIST_MAX];
static u32 attrlist_bytes;
typedef struct { u16 id;uint64_t vcn; } BaseData;
typedef struct {
    MultiFile64 file;
    Run map[RUN_MAX];u32 count;
    int present, resident;u16 flags;
    uint64_t next;
    u8 value[RECORD_MAX];
} DataView;
static DataView view;
static uint64_t r64(const u8 *p){return (uint64_t)r32(p)|((uint64_t)r32(p+4)<<32);}
static int add_extent(const MultiNtfsAttr *a,u32 number){
    if(a->flags&0x4001)return UNSUP;
    if(view.present && (view.resident || !a->nonresident || a->flags!=view.flags))return BAD;
    if(!view.present){
        view.file.data=(Stream){number,0,0,0,0};view.flags=a->flags;
        view.file.size=a->size;view.file.valid=a->initialized;
        if(view.file.valid>view.file.size)return BAD;
        if(!a->nonresident){
            if(a->value_bytes>RECORD_MAX)return UNSUP;
            mcopy(view.value,a->value,a->value_bytes);view.resident=1;view.present=1;return 0;
        }
    }
    if(a->first_vcn!=view.next)return BAD;
    if(a->compression_unit && (!(a->flags&0x8000)||a->compression_unit>4))return UNSUP;
    if(!view.present && !a->size && !a->allocated && a->last_vcn==UINT64_MAX && a->mapping[0]==0){
        view.present=1;return 0;
    }
    MultiNtfsRun decoded[RUN_MAX];u32 count;uint64_t next;
    TRY(ntfs_runs_decode(a->mapping,a->mapping_bytes,a->first_vcn,vol.clusters,
                         decoded,RUN_MAX-view.count,&count,&next));
    if(next>UINT32_MAX)return UNSUP;
    if(a->last_vcn!=next-1)return BAD;
    for(u32 j=0;j<count;j++){
        if(decoded[j].lcn>UINT32_MAX||decoded[j].length>UINT32_MAX)return UNSUP;
        view.map[view.count++]=(Run){(u32)decoded[j].vcn,(u32)decoded[j].lcn,
                                     (u32)decoded[j].length,decoded[j].sparse};
    }
    view.next=next;view.present=1;return 0;
}
static int listed(uint64_t file,u16 id,uint64_t first){
    u32 p=0;MultiNtfsAttrRef ref;int r;
    while((r=ntfs_attrlist_next(attrlist,attrlist_bytes,&p,&ref))>0)
        if(ref.type==0x80&&!ref.name_units&&ref.id==id&&ref.first_vcn==first&&
           (ref.record|((uint64_t)ref.sequence<<48))==file)return 1;
    return r<0?r:0;
}
/* Reloads referenced records; no pointers into the global FILE buffer escape. */
static int stream_info64(u32 number,MultiFile64 *s){
    TRY(load(number));
    if(!(r16(record+22)&1)||r64(record+32))return BAD;
    u16 sequence=r16(record+16),base_flags=r16(record+22);
    uint64_t base_ref=(uint64_t)number|((uint64_t)sequence<<48);
    mzero(&view,sizeof(view));view.file.data.first=number;attrlist_bytes=0;
    BaseData base[RUN_MAX];u32 bases=0,seen=0,off=r16(record+20);
    MultiNtfsAttr a,list={0};int r,have_list=0;
    while((r=ntfs_attr_next(record,record_bytes,&off,&a))>0){
        if(a.type==0xc0)return UNSUP;
        if(a.type==0x20){if(have_list++)return UNSUP;list=a;}
        if(a.type==0x80&&!a.name_units){
            if(bases==RUN_MAX)return UNSUP;
            base[bases++]=(BaseData){a.id,a.first_vcn};
        }
    }
    if(r<0)return r;
    if(!have_list){
        if(bases>1)return BAD;
        off=r16(record+20);
        while((r=ntfs_attr_next(record,record_bytes,&off,&a))>0)
            if(a.type==0x80&&!a.name_units)TRY(add_extent(&a,number));
        if(r<0)return r;
    }else{
        if(list.flags||list.first_vcn||list.size>LIST_MAX||list.initialized!=list.size)return UNSUP;
        attrlist_bytes=(u32)list.size;
        if(!attrlist_bytes)return BAD;
        if(list.nonresident){
            Run map[RUN_MAX];u32 count;
            TRY(runs(list.raw,map,&count,attrlist_bytes,1));
            TRY(runread(map,count,0,attrlist,attrlist_bytes));
        }else mcopy(attrlist,list.value,attrlist_bytes);
        /* Validate the complete list before following a reference. */
        u32 pos=0;MultiNtfsAttrRef ref;
        while((r=ntfs_attrlist_next(attrlist,attrlist_bytes,&pos,&ref))>0){}
        if(r<0)return r;
        pos=0;u32 extents=0;
        while((r=ntfs_attrlist_next(attrlist,attrlist_bytes,&pos,&ref))>0){
            if(ref.type!=0x80||ref.name_units)continue;
            if(++extents>RUN_MAX)return UNSUP;
            if(ref.first_vcn!=view.next || (view.present&&view.resident))return BAD;
            if(ref.record>=mft_records)return UNSUP;
            TRY(load((u32)ref.record));
            if(!(r16(record+22)&1)||r16(record+16)!=ref.sequence||
               ((r16(record+22)^base_flags)&2))return BAD;
            uint64_t parent=r64(record+32);
            if(ref.record==number){if(parent||ref.sequence!=sequence)return BAD;}
            else if(parent!=base_ref)return BAD;
            u32 p=r16(record+20);int found=0;
            while((r=ntfs_attr_next(record,record_bytes,&p,&a))>0){
                if(a.type==0xc0)return UNSUP;
                if(a.type==0x20&&ref.record!=number)return BAD;
                if(a.type==0x80&&!a.name_units){
                    int included=listed((uint64_t)ref.record|((uint64_t)ref.sequence<<48),a.id,a.first_vcn);
                    if(included<0)return included;
                    if(!included)return BAD;
                }
                if(a.id!=ref.id)continue;
                if(found++||a.type!=0x80||a.name_units||a.first_vcn!=ref.first_vcn)return BAD;
                TRY(add_extent(&a,number));
            }
            if(r<0)return r;
            if(!found)return BAD;
            if(ref.record==number){
                int matched=0;
                for(u32 j=0;j<bases;j++)if(base[j].id==ref.id&&base[j].vcn==ref.first_vcn){
                    if(seen&(1u<<j))return BAD;
                    seen|=1u<<j;matched++;
                }
                if(matched!=1)return BAD;
            }
        }
        if(r<0)return r;
        if(seen!=(bases==32?UINT32_MAX:((1u<<bases)-1)))return BAD;
    }
    u32 shift=0;while((1u<<shift)<vol.cluster_bytes)shift++;
    if(view.present&&!view.resident&&view.file.size&&
       1+((view.file.size-1)>>shift)>view.next)return BAD;
    view.file.data.size=view.file.size>UINT32_MAX?UINT32_MAX:(u32)view.file.size;
    view.file.data.valid=view.file.valid>UINT32_MAX?UINT32_MAX:(u32)view.file.valid;
    if(view.file.size>UINT32_MAX||view.file.valid>UINT32_MAX)view.file.data.flags|=MULTI_STREAM_WIDE;
    *s=view.file;return 0;
}
static int stream_info(u32 number,Stream *s){
    MultiFile64 f;TRY(stream_info64(number,&f));
    if(f.size>UINT32_MAX||f.valid>UINT32_MAX)return UNSUP;
    *s=f.data;return 0;
}
Stream ntfs_root(void) {
    Stream s = {5, 0, 0, ATTR_DIR, 1};
    return s;
}
int ntfs_mount_at(u32 base, u32 limit) {
    ntfs_case_valid=0;mzero(&ntfs_upcase_stream,sizeof(ntfs_upcase_stream));
    mzero(&ntfs_io,sizeof(ntfs_io));
    MultiBlock64Provider provider={.abi=MULTIOS_BLOCK64_ABI,.sector_bytes=512,
        .sectors=disk_sectors(),.read=legacy_read};
    TRY(multi_block64_bind(&ntfs_io,&provider));
    mft_run_count = 0;
    mft_records = 0;
    u8 b[512];
    TRY(disk_read(base, b));
    if (b[510] != 0x55 || b[511] != 0xaa || r16(b + 11) != 512 || !b[13] || b[13] > 128 ||
        (b[13] & (b[13] - 1)))
        return UNSUP;
    if (r32(b + 44) || r32(b + 52) || r32(b + 60))
        return UNSUP;
    vol.base = base;
    vol.sectors = r32(b + 40);
    vol.spc = b[13];
    vol.cluster_bytes = vol.spc * 512;
    /* NTFS stores the last usable primary-volume sector count; backup is often at that LBA. */
    if (!vol.sectors || vol.sectors > limit || base >= 0x10000000u ||
        vol.sectors > 0x10000000u - base)
        return BAD;
    vol.clusters = vol.sectors / vol.spc;
    vol.root = 5;
    vol.heap = 0;
    vol.fat = 0;
    int8_t code = (int8_t)b[64];
    if (code < 0) {
        u32 shift = -(int)code;
        if (shift < 9 || shift > 12)
            return UNSUP;
        record_bytes = 1u << shift;
    } else {
        if (!code || code > RECORD_MAX / vol.cluster_bytes)
            return UNSUP;
        record_bytes = (u32)code * vol.cluster_bytes;
    }
    if (record_bytes > RECORD_MAX || record_bytes < 512 || (record_bytes & (record_bytes - 1)))
        return UNSUP;
    u32 lcn = r32(b + 48);
    if (lcn >= vol.clusters || record_bytes / 512 > vol.sectors - lcn * vol.spc)
        return BAD;
    for (u32 j = 0; j < record_bytes / 512; j++)
        TRY(rd(lcn * vol.spc + j, record + j * 512));
    TRY(record_ok());
    if (!(r16(record + 22) & 1) || r32(record + 32) || r32(record + 36))
        return BAD;
    const u8 *a;
    TRY(data_attr(&a));
    if (!a || !a[8] || r32(a + 52) || r32(a + 60))
        return UNSUP;
    mft_size = r32(a + 48);
    mft_valid = r32(a + 56);
    if (mft_valid > mft_size || mft_size % record_bytes || mft_valid < 16 * record_bytes)
        return BAD;
    mft_records = mft_valid / record_bytes;
    if (mft_records > MFT_MAX)
        return UNSUP;
    TRY(runs(a, mft_runs, &mft_run_count, mft_size, 1));
    TRY(load(3));
    u32 off = r16(record + 20);
    const u8 *info;
    int r;
    int have = 0;
    while ((r = attr_next(&off, &info)) > 0)
        if (r32(info) == 0x70 && !info[8]) {
            u32 n = r32(info + 16);
            const u8 *v = info + r16(info + 20);
            if (n < 12)
                return BAD;
            if (v[8] != 3 || v[9] > 1)
                return UNSUP;
            vol.flags = (r16(v + 10) & 1) ? 2 : 0;
            have = 1;
        }
    if (r < 0)
        return r;
    if (!have)
        return BAD;
    TRY(stream_info(10,&ntfs_upcase_stream));
    if(ntfs_upcase_stream.size!=131072||ntfs_upcase_stream.valid!=131072)return BAD;
    TRY(ntfs_read(ntfs_upcase_stream,0,ntfs_case_cache,512));
    for(u32 i=0;i<128;i++)if(r16(ntfs_case_cache+i*2)!=upper(i))return BAD;
    ntfs_case_page=0;ntfs_case_valid=1;
    TRY(load(5));
    if ((r16(record + 22) & 3) != 3 || r64(record+32))
        return BAD;
    return 0;
}
int ntfs_read64(const MultiFile64 *f,uint64_t off,u8 *b,u32 n){
    if(!f||(n&&!b))return INVALID;
    if(f->valid>f->size||off>f->size||(uint64_t)n>f->size-off)return BAD;
    if(!n)return 0;
    TRY(load(f->data.first));if(!(r16(record+22)&1))return BAD;
    if(r16(record+22)&2)return ISDIR;
    MultiFile64 actual;TRY(stream_info64(f->data.first,&actual));
    if(actual.size!=f->size||actual.valid!=f->valid||!view.present)return BAD;
    uint64_t left=off<f->valid?f->valid-off:0;u32 initialized=left>n?n:(u32)left;
    if(initialized){
        if(view.resident)mcopy(b,view.value+(u32)off,initialized);
        else TRY(runread(view.map,view.count,off,b,initialized));
    }
    if(n>initialized)mzero(b+initialized,n-initialized);return 0;
}
int ntfs_read(Stream s,u32 off,u8 *b,u32 n){
    if(s.flags&MULTI_STREAM_WIDE)return UNSUP;
    MultiFile64 f={.data=s,.size=s.size,.valid=s.valid};return ntfs_read64(&f,off,b,n);
}
static int ntfs_upcase(u16 c,u16 *v){
 u32 page=(u32)c>>8;if(!ntfs_case_valid||ntfs_case_page!=page){
  ntfs_case_valid=0;TRY(ntfs_read(ntfs_upcase_stream,page*512,ntfs_case_cache,512));
  ntfs_case_page=page;ntfs_case_valid=1;}
 *v=r16(ntfs_case_cache+(c&255)*2);return 0;
}
static int ntfs_equal(const char *a,const char *b){return multi_name_equal(a,b,ntfs_upcase);}
int ntfs_next(Stream d, u32 *cursor, Entry *e) {
    if (!cursor || !e)
        return INVALID;
    TRY(load(d.first));
    if ((r16(record + 22) & 3) != 3 || r64(record+32))
        return NOTDIR;
    u16 sequence = r16(record + 16);
    u32 number = *cursor >> 8, wanted = *cursor & 255;
    if (number > mft_records)
        return BAD;
    u32 scans = 0;
    for (; number < mft_records; number++, wanted = 0) {
        if (++scans > SCAN_MAX)
            return UNSUP;
        /* Unused MFT slots are allowed to contain zero or stale bytes. Read raw flags
         * first; fixups are required only for in-use records. */
        u32 offset = number * record_bytes;
        TRY(runread(mft_runs, mft_run_count, offset, record, record_bytes));
        if (!eq4(record, "FILE")) {
            if (eq4(record, "BAAD"))
                return BAD;
            continue;
        }
        if (!(r16(record + 22) & 1))
            continue;
        TRY(record_ok());
        if (r32(record + 32) || r32(record + 36))
            continue;
        if (number < 16)
            continue; /* Hide NTFS internal metadata, including root's self-reference. */
        u32 p = r16(record + 20), ordinal = 0;
        const u8 *a;
        int r;
        while ((r = attr_next(&p, &a)) > 0) {
            if (r32(a) != 0x30)
                continue;
            if (ordinal++ < wanted)
                continue;
            if (ordinal > 255)
                return UNSUP;
            if (a[8])
                return BAD;
            u32 size = r32(a + 16);
            const u8 *v = a + r16(a + 20);
            if (size < 66)
                return BAD;
            u32 len = v[64];
            if (!len || len > 255 || len * 2 > size - 66 || v[65] > 3)
                return BAD;
            if (v[65] == 2)
                continue; /* DOS alias duplicates are not enumerated. */
            if (r16(v + 4) || r32(v) != d.first)
                continue;
            if (r16(v + 6) != sequence)
                return BAD;
            mzero(e, sizeof(*e));
            u16 name[255];for(u32 j=0;j<len;j++)name[j]=r16(v+66+j*2);
            TRY(multi_name_entry(name,len,e));
            e->attr = (u16)(r32(v + 56) & 0x27);
            if (r16(record + 22) & 2)
                e->attr |= ATTR_DIR;
            e->pos = number;
            e->count = 1;
            if (r32(v + 56) & 0x400)
                e->large = MULTI_ENTRY_FEATURE;
            if (e->attr & ATTR_DIR) {
                Stream check;
                int sr = stream_info(number, &check);
                if (sr == UNSUP)
                    e->large = MULTI_ENTRY_FEATURE;
                else if (sr < 0)
                    return sr;
                e->data = (Stream){number, 0, 0, ATTR_DIR, 0};
            } else {
                MultiFile64 f;
                r = stream_info64(number, &f);
                if(!r){e->data=f.data;e->size64=f.size;e->valid64=f.valid;e->wide=1;
                    if(f.size>UINT32_MAX||f.valid>UINT32_MAX)e->large|=MULTI_ENTRY_SIZE64;}
                if (r == UNSUP) {
                    e->large = MULTI_ENTRY_FEATURE;
                    e->data.first = number;
                } else if (r < 0)
                    return r;
            }
            *cursor = (number << 8) | ordinal;
            return 1;
        }
        if (r < 0)
            return r;
    }
    *cursor = mft_records << 8;
    return 0;
}
int ntfs_free(u32 *count) {
    if (!count)
        return INVALID;
    TRY(load(6));
    Stream bitmap;
    TRY(stream_info(6, &bitmap));
    u32 need = (vol.clusters + 7) / 8;
    if (bitmap.size < need)
        return BAD;
    u8 b[512];
    *count = 0;
    for (u32 off = 0; off < need; off += 512) {
        u32 n = need - off;
        if (n > 512)
            n = 512;
        TRY(ntfs_read(bitmap, off, b, n));
        for (u32 j = 0; j < n; j++)
            for (u32 bit = 0; bit < 8; bit++)
                if ((off + j) * 8 + bit < vol.clusters && !(b[j] & (1u << bit)))
                    (*count)++;
    }
    return 0;
}
int ntfs_unlock(void) {
    vol.writable = 0;
    return READONLY;
}
int ntfs_put(const char *p, const u8 *b, u32 n, int m, const char *s) {
    (void)p;
    (void)b;
    (void)n;
    (void)m;
    (void)s;
    return READONLY;
}
int ntfs_remove(const char *p, int d) {
    (void)p;
    (void)d;
    return READONLY;
}
int ntfs_rename(const char *p, const char *n) {
    (void)p;
    (void)n;
    return READONLY;
}

/* Only the descriptor and profile know this backend's concrete entry points. */
static int ntfs_probe(const u8 *b){for(u32 i=0;i<8;i++)if(b[3+i]!=(u8)"NTFS    "[i])return 0;return 1;}
static int ntfs_priority(u8 t){return t==7?100:0;}
#ifdef MULTIOS_NTFS3G
const MultiFsDriver multi_fs_ntfs={
 .abi=MULTIOS_FS_ABI,.size=sizeof(MultiFsDriver),.kind=FS_NTFS,.id="ntfs",.name="NTFS",
 .capabilities=MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_EXPERIMENTAL|MULTIOS_FS_RANDOM_IO|MULTIOS_FS_NAMED_STREAMS|MULTIOS_FS_HARDLINKS|MULTIOS_FS_EXACT_NAMES,
 .probe=ntfs_probe,.partition_priority=ntfs_priority,.mount=multi_ntfs3g_mount,
 .root=ntfs_root,.next=multi_ntfs3g_next,.read=multi_ntfs3g_read,.read64=multi_ntfs3g_read64,.name_equal=multi_ntfs3g_equal,.free=multi_ntfs3g_free,
 .unlock=multi_ntfs3g_unlock,.put=multi_ntfs3g_put,.remove=multi_ntfs3g_remove,.rename=multi_ntfs3g_rename,
 .write_at=multi_ntfs3g_write_at,.resize=multi_ntfs3g_resize,
 .stream_next=multi_ntfs3g_stream_next,.stream_read=multi_ntfs3g_stream_read,
 .stream_change=multi_ntfs3g_stream_change,.link=multi_ntfs3g_link,
};

#else
const MultiFsDriver multi_fs_ntfs={
 .abi=MULTIOS_FS_ABI,.size=sizeof(MultiFsDriver),.kind=FS_NTFS,.id="ntfs",.name="NTFS",
 .capabilities=MULTIOS_FS_READ,
 .probe=ntfs_probe,.partition_priority=ntfs_priority,.mount=ntfs_mount_at,
 .root=ntfs_root,.next=ntfs_next,.read=ntfs_read,.read64=ntfs_read64,.name_equal=ntfs_equal,.free=ntfs_free,
};

#endif
