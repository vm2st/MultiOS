/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
/* MultiOS experimental exFAT driver. No TexFAT, GPT, Unicode lookup or journal.
 * All on-disk integers are decoded explicitly; no packed struct aliasing. */
#include "multi.h"
#include "fs_backend.h"
#include "unicode.h"
#define fs_root exfat_root
#define fs_next exfat_next
#define fs_read exfat_read
#define fs_free exfat_free
#define fs_unlock exfat_unlock
#define fs_put exfat_put
#define fs_remove exfat_remove
#define fs_rename exfat_rename
/* Bounded, allocation-free write preflight. Unsupported/ambiguous volumes stay RO.
 * A consistency check, not a repair tool or complete exFAT validator. */
#if defined(MULTIOS_FATFS) && defined(__x86_64__)
/* Larger audit workspace is host/UEFI-only: preserve the BIOS low-memory ABI. */
#define AUDIT_CLUSTERS 1048576u
#else
#define AUDIT_CLUSTERS 65536u
#endif
#define AUDIT_RECORDS 32768u
#define AUDIT_DEPTH 16u
static u8 audit_seen[AUDIT_CLUSTERS/8],audit_bitmap[AUDIT_CLUSTERS/8];
static u32 audit_budget;
static int strict_walk;
static int validname(const char *);
static int audit_volume(void);
static u16 r16(const u8 *p) {
    return p[0] | ((u16)p[1] << 8);
}
static u32 r32(const u8 *p) {
    return r16(p) | ((u32)r16(p + 2) << 16);
}
static uint64_t r64(const u8*p){return (uint64_t)r32(p)|((uint64_t)r32(p+4)<<32);}
static void w16(u8 *p, u16 n) {
    p[0] = n;
    p[1] = n >> 8;
}
static void w32(u8 *p, u32 n) {
    w16(p, n);
    w16(p + 2, n >> 16);
}
static int validcl(u32 c) {
    return c >= 2 && c - 2 < vol.clusters;
}
static int rd(u32 rel, u8 *b) {
    if (rel >= vol.sectors || rel > 0x0fffffffu - vol.base)
        return BAD;
    return disk_read(vol.base + rel, b);
}
static int wr(u32 rel, const u8 *b) {
    if (!vol.writable || vol.poisoned)
        return READONLY;
    if (rel >= vol.sectors || rel > 0x0fffffffu - vol.base)
        return BAD;
    int r = disk_write(vol.base + rel, b);
    if (r < 0)
        vol.poisoned = 1;
    return r;
}
static u32 clsec(u32 c) {
    return vol.heap + (c - 2) * vol.spc;
}
static int fatget(u32 c, u32 *n) {
    u8 b[512];
    if (!validcl(c))
        return BAD;
    TRY(rd(vol.fat + c / 128, b));
    *n = r32(b + (c % 128) * 4);
    return 0;
}
static int fatset(u32 c, u32 n) {
    u8 b[512];
    if (!validcl(c))
        return BAD;
    TRY(rd(vol.fat + c / 128, b));
    w32(b + (c % 128) * 4, n);
    return wr(vol.fat + c / 128, b);
}
/* Every chain walk is bounded, including a directory with a cyclic FAT. */
static int cluster(Stream s, u32 index, u32 *c) {
    if (index >= vol.clusters || !validcl(s.first))
        return BAD;
    *c = s.first;
    if (s.flags & 2) {
        if (index > vol.clusters + 1 - *c)
            return BAD;
        *c += index;
        return validcl(*c) ? 0 : BAD;
    }
    while (index--) {
        u32 n;
        TRY(fatget(*c, &n));
        if (!validcl(n))
            return BAD;
        *c = n;
    }
    return 0;
}
static int byteio(Stream s, u32 off, u8 *b, u32 n, int write) {
    if (!s.root && (off > s.size || n > s.size - off))
        return BAD;
    while (n) {
        u32 c, part = off % 512, take = 512 - part;
        u8 sec[512];
        if (take > n)
            take = n;
        TRY(cluster(s, off / vol.cluster_bytes, &c));
        u32 lba = clsec(c) + (off % vol.cluster_bytes) / 512;
        TRY(rd(lba, sec));
        if (write) {
            mcopy(sec + part, b, take);
            TRY(wr(lba, sec));
        } else
            mcopy(b, sec + part, take);
        off += take;
        b += take;
        n -= take;
    }
    return 0;
}
int fs_read(Stream s, u32 off, u8 *b, u32 n) {
    if(s.flags&MULTI_STREAM_WIDE)return UNSUP;
    if (s.valid > s.size || off > s.size || n > s.size - off)
        return BAD;
    if (n && !b) return INVALID;
    u32 real = off < s.valid ? s.valid - off : 0;
    if (real > n)
        real = n;
    if (real)
        TRY(byteio(s, off, b, real, 0));
    if (n > real) mzero(b + real, n - real);
    return 0;
}
/* Read-only wide path. No truncation to the legacy byteio/write interface. */
int exfat_read64(const MultiFile64 *f,uint64_t off,u8 *b,u32 n){
    if(!f||(n&&!b))return INVALID;
    if(f->valid>f->size||off>f->size||(uint64_t)n>f->size-off)return BAD;
    if(f->data.root || (f->data.flags&~(3|MULTI_STREAM_WIDE)) || !(f->data.flags&1))return BAD;
    if(f->size>(uint64_t)vol.clusters*vol.cluster_bytes)return BAD;
    if(f->size&&!validcl(f->data.first))return BAD;
    if((f->data.flags&2)&&f->size>(uint64_t)(vol.clusters+2-f->data.first)*vol.cluster_bytes)return BAD;
    uint64_t left=off<f->valid?f->valid-off:0;u32 real=left>n?n:(u32)left;
    u32 initialized=real,shift=0;while((1u<<shift)<vol.cluster_bytes)shift++;
    while(real){
        uint64_t index=off>>shift;if(index>UINT32_MAX)return UNSUP;
        u32 c,part=(u32)off&511,take=512-part;u8 sector[512];if(take>real)take=real;
        TRY(cluster(f->data,(u32)index,&c));
        TRY(rd(clsec(c)+(((u32)off&(vol.cluster_bytes-1))>>9),sector));
        mcopy(b,sector+part,take);b+=take;off+=take;real-=take;
    }
    if(n>initialized)mzero(b,n-initialized);return 0;
}
static u16 setchecksum(const u8 *b, u32 n) {
    u16 s = 0;
    for (u32 i = 0; i < n; i++)
        if (i != 2 && i != 3)
            s = (u16)((s >> 1) | (s << 15)) + b[i];
    return s;
}
typedef struct {u32 offset,skip;} UpCheckpoint;
static UpCheckpoint up_index[256];
static u8 up_cache[512];static u32 up_cache_block;static int up_ready,up_cache_valid;
static int up_word(u32 off,u16 *v){
 if(off>vol.upcase.size||vol.upcase.size-off<2)return BAD;
 u32 block=off&~511u;if(!up_cache_valid||up_cache_block!=block){
  u32 n=vol.upcase.size-block;if(n>512)n=512;
  up_cache_valid=0;TRY(byteio(vol.upcase,block,up_cache,n,0));up_cache_block=block;up_cache_valid=1;}
 *v=r16(up_cache+(off&511));return 0;
}
static int up_decode(u32 c,u32 *off,u32 *skip,u16 *v){
 if(*skip){(*skip)--;*v=(u16)c;return 0;}
 u16 w;TRY(up_word(*off,&w));*off+=2;
 if(w==0xffff&&c!=0xffff){u16 count;TRY(up_word(*off,&count));*off+=2;
  if(!count||(u32)count>65536u-c)return BAD;*skip=count-1;*v=(u16)c;
 }else *v=w;return 0;
}
static int up_build(void){
 up_ready=up_cache_valid=0;if(vol.upcase.size&1)return BAD;
 u32 off=0,skip=0;for(u32 c=0;c<65536;c++){
  if(!(c&255))up_index[c>>8]=(UpCheckpoint){off,skip};
  u16 v;TRY(up_decode(c,&off,&skip,&v));
  if(c<128&&v!=upper(c))return UNSUP;
  /* Surrogates must remain themselves; never create invalid UTF-16 mappings. */
  if((c>=0xd800&&c<=0xdfff)?v!=c:(v>=0xd800&&v<=0xdfff))return BAD;
 }
 if(skip||off!=vol.upcase.size)return BAD;up_ready=1;return 0;
}
static int exfat_upcase(u16 c,u16 *v){
 if(!up_ready)return BAD;UpCheckpoint p=up_index[c>>8];
 for(u32 i=c&~255u;i<=c;i++)TRY(up_decode(i,&p.offset,&p.skip,v));return 0;
}
#ifdef MULTIOS_FATFS
int multi_exfat_upcase(u16 c,u16*v){return exfat_upcase(c,v);}
#endif
static int exfat_equal(const char *a,const char *b){return multi_name_equal(a,b,exfat_upcase);}
static int namehash16(const u16 *n,u32 count,u16 *result){
 u16 h=0;for(u32 i=0;i<count;i++){u16 c;TRY(exfat_upcase(n[i],&c));
 h=(u16)((h>>1)|(h<<15))+(c&255);h=(u16)((h>>1)|(h<<15))+(c>>8);}
 *result=h;return 0;
}
static int namehash(const char *n,u16 *h){u16 text[255];u32 count;TRY(multi_utf8_to16(n,text,255,&count));return namehash16(text,count,h);}
Stream fs_root(void) {
    Stream s = {vol.root, 0, 0, 0, 1};
    return s;
}
static int dirsize(Stream s, u32 *n) {
    if (!s.root) {
        if (!s.size || s.size % vol.cluster_bytes || s.valid != s.size || !validcl(s.first)) return BAD;
        *n = s.size;
        return 0;
    }
    u32 c = s.first;
    *n = 0;
    for (u32 k = 0; k < vol.clusters; k++) {
        if (!validcl(c) || *n > 0xffffffffu - vol.cluster_bytes)
            return BAD;
        *n += vol.cluster_bytes;
        u32 next;
        TRY(fatget(c, &next));
        if (next >= 0xfffffff8u)
            return 0;
        c = next;
    }
    return BAD;
}
int fs_next(Stream d, u32 *cursor, Entry *e) {
    if (!cursor || !e) return INVALID;
    u32 size;
    TRY(dirsize(d, &size));
    if (*cursor > size || (*cursor & 31)) return BAD;
    u8 b[608];
    while (*cursor <= size && size - *cursor >= 32) {
        if (strict_walk) { if (!audit_budget) return UNSUP; audit_budget--; }
        u32 p = *cursor;
        TRY(byteio(d, p, b, 32, 0));
        *cursor += 32;
        if (b[0] == 0)
            return 0;
        if (b[0] != 0x85) {
            if (strict_walk && (b[0]&0x80) &&
                !(d.root && (b[0]==0x81 || b[0]==0x82 || b[0]==0x83))) return UNSUP;
            if ((b[0] & 0xe0) == 0x80 && b[0] != 0x81 && b[0] != 0x82 && b[0] != 0x83)
                return UNSUP;
            continue;
        }
        u32 n = b[1] + 1;
        if (n < 3 || n > 19 || n * 32 > size - p)
            return BAD;
        if (strict_walk) { if (n-1 > audit_budget) return UNSUP; audit_budget -= n-1; }
        TRY(byteio(d, p, b, n * 32, 0));
        if (r16(b + 2) != setchecksum(b, n * 32) || b[32] != 0xc0)
            return BAD;
        u32 len = b[35], chars = 0;u16 text[255];
        if (strict_walk && n != 2 + (len + 14) / 15) return UNSUP;
        if (!len || n < 2 + (len + 14) / 15)
            return BAD;
        mzero(e, sizeof(*e));
        e->ascii = 1;
        for (u32 i = 2; i < n; i++) {
            if (b[i * 32] != 0xc1) {
                if (strict_walk) return UNSUP;
                if ((b[i * 32] & 0xe0) == 0xc0)
                    return UNSUP;
                continue;
            }
            for (u32 j = 0; j < 15 && chars < len; j++) {
                u16 ch = r16(b + i * 32 + 2 + j * 2);
                if (ch < 32 || ch > 126)
                    e->ascii = 0;
                text[chars++] = ch;
            }
        }
        if (chars != len)
            return BAD;
        TRY(multi_name_entry(text,len,e));
        e->attr = r16(b + 4);
        e->data.flags = b[33];
        if ((b[33] & 1) == 0 || (b[33] & ~3))
            return UNSUP;
        e->data.first = r32(b + 52);
        e->data.valid = r32(b + 40);
        e->data.size = r32(b + 56);
        e->size64=r64(b+56);e->valid64=r64(b+40);e->wide=1;
        e->large=(e->size64>UINT32_MAX||e->valid64>UINT32_MAX)?MULTI_ENTRY_SIZE64:0;
        if(e->valid64>e->size64 || (e->size64&&!validcl(e->data.first)))return BAD;
        /* A single exFAT stream cannot own more data clusters than this volume. */
        if(e->size64>(uint64_t)vol.clusters*vol.cluster_bytes)return BAD;
        if((e->data.flags&2)&&e->size64>(uint64_t)(vol.clusters+2-e->data.first)*vol.cluster_bytes)return BAD;
        if(e->large)e->data.flags|=MULTI_STREAM_WIDE;
        e->pos = p;
        e->count = n;
        if (!e->large &&
            (e->data.valid > e->data.size || (e->data.size && !validcl(e->data.first))))
            return BAD;
        u16 hash;TRY(namehash16(text,len,&hash));if(r16(b+36)!=hash)return BAD;
        *cursor = p + n * 32;
        return 1;
    }
    return 0;
}
static int validname(const char *n) {return multi_name_valid(n);}
static int bitmapbit(u32 cl, int value, int *old) {
    if (!validcl(cl)) return BAD;
    u8 b;
    u32 off = (cl - 2) / 8;
    TRY(byteio(vol.bitmap, off, &b, 1, 0));
    u8 mask = 1u << ((cl - 2) % 8);
    if (old)
        *old = !!(b & mask);
    if (value >= 0) {
        b = value ? (b | mask) : (b & ~mask);
        TRY(byteio(vol.bitmap, off, &b, 1, 1));
    }
    return 0;
}
int fs_free(u32 *count) {
    u8 b[512];
    *count = 0;
    u32 bytes = (vol.clusters + 7) / 8;
    for (u32 off = 0; off < bytes; off += 512) {
        u32 n = bytes - off;
        if (n > 512)
            n = 512;
        TRY(byteio(vol.bitmap, off, b, n, 0));
        for (u32 j = 0; j < n; j++)
            for (u32 bit = 0; bit < 8; bit++)
                if ((off + j) * 8 + bit < vol.clusters && !(b[j] & (1 << bit)))
                    (*count)++;
    }
    return 0;
}
static int setflags(u16 flags) {
    u8 b[512];
    TRY(rd(0, b));
    w16(b + 106, flags);
    b[112] = 255;
    TRY(wr(0, b));
    TRY(disk_flush());
    vol.flags = flags;
    return 0;
}
int fs_unlock(void) {
    if (!vol.mounted)
        return BAD;
    vol.writable = 0;
    if (!disk_writable())
        return READONLY;
    if (vol.flags & 0xfffe || vol.poisoned)
        return BAD;
    int r = audit_volume();
    if (r < 0) return r;
    vol.writable = 1;
    return 0;
}
static int begin(void) {
    if (!vol.writable || vol.poisoned)
        return READONLY;
    if (vol.flags & 2)
        return BAD;
    int r = setflags(vol.flags | 2);
    if (r < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
    }
    return r;
}
static int finish(int r) {
    if (r < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
        return r;
    }
    if (disk_flush() < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
        return IO;
    }
    r = setflags(vol.flags & ~2);
    if (r < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
    }
    return r;
}
static int allocchain(u32 count, Stream *s) {
    mzero(s, sizeof(*s));
    if (!count)
        return 0;
    u32 prev = 0, left = count;
    for (u32 c = 2; c < vol.clusters + 2 && left; c++) {
        int used;
        TRY(bitmapbit(c, -1, &used));
        if (used)
            continue;
        TRY(bitmapbit(c, 1, 0));
        TRY(fatset(c, EOC));
        if (prev)
            TRY(fatset(prev, c));
        else
            s->first = c;
        prev = c;
        left--;
        u8 zero[512];
        mzero(zero, 512);
        for (u32 j = 0; j < vol.spc; j++)
            TRY(wr(clsec(c) + j, zero));
    }
    if (left)
        return NOSPACE;
    s->size = count * vol.cluster_bytes;
    s->valid = s->size;
    s->flags = 1;
    return 0;
}
static int freechain(Stream s) {
    if (!s.size)
        return 0;
    u32 count = 1 + (s.size - 1) / vol.cluster_bytes, c = s.first;
    for (u32 i = 0; i < count; i++) {
        if (!validcl(c))
            return BAD;
        u32 next = 0;
        if (i + 1 < count) {
            if (s.flags & 2)
                next = c + 1;
            else
                TRY(fatget(c, &next));
        }
        TRY(bitmapbit(c, 0, 0));
        TRY(fatset(c, 0));
        c = next;
    }
    return 0;
}
static int freeslot(Stream d, u32 n, u32 *pos) {
    u32 sz, run = 0;
    u8 b[32];
    TRY(dirsize(d, &sz));
    int at_end = 0;
    for (u32 p = 0; p <= sz && sz - p >= 32; p += 32) {
        TRY(byteio(d, p, b, 32, 0));
        if (b[0] == 0)
            at_end = 1;
        if (at_end || !(b[0] & 0x80))
            run++;
        else
            run = 0;
        if (run == n) {
            *pos = p + 32 - n * 32;
            return 0;
        }
    }
    return NOSPACE;
}
static int makeentry(u8 *b, const char *name, Stream s, u16 attr) {
    u16 text[255],hash;u32 l;TRY(multi_utf8_to16(name,text,255,&l));TRY(namehash(name,&hash));
    u32 n=2+(l+14)/15;
    mzero(b, n * 32);
    b[0] = 0x85;
    b[1] = n - 1;
    w16(b + 4, attr); /* valid DOS date 1980-01-01 */
    w16(b + 10, 0x21);
    w16(b + 14, 0x21);
    w16(b + 18, 0x21);
    b[32] = 0xc0;
    b[33] = s.flags ? s.flags : 1;
    b[35] = l;
    w16(b + 36, hash);
    w32(b + 40, s.valid);
    w32(b + 52, s.first);
    w32(b + 56, s.size);
    for (u32 i = 0; i < l; i++) {
        u32 entry = 2 + i / 15;
        b[entry * 32] = 0xc1;
        w16(b + entry * 32 + 2 + (i % 15) * 2, text[i]);
    }
    w16(b + 2, setchecksum(b, n * 32));return 0;
}
/* Publish secondaries before primary. Not a journal: failure poisons the mount. */
static int publish(Stream d, u32 pos, u8 *b, u32 count) {
    u8 entry[32];
    int crossed_end = 0;
    u32 size;
    TRY(dirsize(d, &size));
    for (u32 i = 0; i < count; i++) {
        TRY(byteio(d, pos + i * 32, entry, 32, 0));
        if (!entry[0])
            crossed_end = 1;
    }
    if (crossed_end && pos + count * 32 < size) {
        mzero(entry, 32);
        TRY(byteio(d, pos + count * 32, entry, 32, 1));
    }
    TRY(byteio(d, pos + 32, b + 32, (count - 1) * 32, 1));
    TRY(disk_flush());
    return byteio(d, pos, b, 32, 1);
}
/* mode: 0 touch(no overwrite), 1 replace, 2 append, 3 mkdir, 4 copy(no overwrite) */
int fs_put(const char *path, const u8 *text, u32 len, int mode, const char *source) {
    if (mode<0 || mode>4 || (len && !text && mode!=3 && mode!=4) || (mode==4 && !source)) return INVALID;
    /* Do not create a directory that our own next write preflight cannot visit. */
    if(mode==3) { u32 depth=0;for(const char *p=path;*p;p++)if(*p=='/')depth++;if(depth>AUDIT_DEPTH)return UNSUP; }
    Stream parent;
    char name[256];
    Entry old, src;
    int found;
    TRY(fs_parent(path, &parent, name));
    TRY(validname(name));
    int lr = fs_lookup(parent, name, &old);
    if (lr < 0 && lr != NOTFOUND)
        return lr;
    found = lr == 0;
    if (found && (mode == 0 || mode == 3 || mode == 4))
        return EXISTS;
    if (found && (old.attr & ATTR_DIR))
        return ISDIR;
    if (found && (old.attr & 1))
        return READONLY;
    if (found && old.count != 2 + (multi_name_units(old.name) + 14) / 15)
        return UNSUP;
    u32 total = len, keep = 0;
    Stream input;
    mzero(&input, sizeof(input));
    if (mode == 4) {
        Stream pp;
        int root;
        TRY(fs_resolve(source, &pp, &src, &root));
        if (src.attr & ATTR_DIR)
            return ISDIR;
        input = src.data;
        keep = input.size;
        total = keep;
    }
    if (mode == 2 && found) {
        input = old.data;
        keep = input.size;
        if (len > 0xffffffffu - keep)
            return UNSUP;
        total = keep + len;
    }
    if (mode == 3)
        total = vol.cluster_bytes;
    if (total > 0xffff0000u)
        return UNSUP;
    u32 required = total ? 1 + (total - 1) / vol.cluster_bytes : 0, free;
    TRY(fs_free(&free));
    if (free < required)
        return NOSPACE;
    u32 cnt = 2 + (multi_name_units(name) + 14) / 15, pos;
    if (found)
        pos = old.pos;
    else
        TRY(freeslot(parent, cnt, &pos));
    TRY(begin());
    Stream news;
    int r = allocchain(required, &news);
    if (r < 0)
        return finish(r);
    u8 buf[512];
    u32 off = 0;
    while (off < keep) {
        u32 n = keep - off;
        if (n > 512)
            n = 512;
        r = fs_read(input, off, buf, n);
        if (r < 0)
            return finish(r);
        r = byteio(news, off, buf, n, 1);
        if (r < 0)
            return finish(r);
        off += n;
    }
    if (mode != 3 && mode != 4 && len) {
        r = byteio(news, keep, (u8 *)text, len, 1);
        if (r < 0)
            return finish(r);
    }
    news.size = total;
    news.valid = total;
    u8 set[608];
    r=makeentry(set, found ? old.name : name, news,
              mode == 3 ? ATTR_DIR : (found ? (old.attr | 32) : 32));if(r<0)return finish(r);
    /* For existing entries the number/name secondaries remain unchanged. */
    r = publish(parent, pos, set, cnt);
    if (r < 0)
        return finish(r);
    if (found) {
        r = freechain(old.data);
        if (r < 0)
            return finish(r);
    }
    return finish(0);
}
int fs_remove(const char *path, int dir) {
    Stream p;
    Entry e;
    int root;
    TRY(fs_resolve(path, &p, &e, &root));
    if (root)
        return INVALID;
    if (!!(e.attr & ATTR_DIR) != !!dir)
        return dir ? NOTDIR : ISDIR;
    if (e.attr & 1)
        return READONLY;
    if (dir) {
        u32 off = 0;
        Entry child;
        int r = fs_next(e.data, &off, &child);
        if (r < 0)
            return r;
        if (r)
            return NOTEMPTY;
    }
    TRY(begin());
    u8 b[32];
    for (u32 j = 0; j < e.count; j++) {
        int r = byteio(p, e.pos + j * 32, b, 32, 0);
        if (r < 0)
            return finish(r);
        b[0] &= 0x7f;
        r = byteio(p, e.pos + j * 32, b, 32, 1);
        if (r < 0)
            return finish(r);
    }
    int r = disk_flush();
    if (r < 0)
        return finish(r);
    r = freechain(e.data);
    return finish(r);
}
int fs_rename(const char *path, const char *name) {
    TRY(validname(name));
    Stream p;
    Entry e, other;
    int root;
    TRY(fs_resolve(path, &p, &e, &root));
    if (root)
        return INVALID;
    if (e.attr & 1)
        return READONLY;
    if (e.count != 2 + (multi_name_units(e.name) + 14) / 15)
        return UNSUP;
    int r = fs_lookup(p, name, &other);
    if (r < 0 && r != NOTFOUND)
        return r;
    if (r == 0 && other.pos != e.pos)
        return EXISTS;
    u32 cnt = 2 + (multi_name_units(name) + 14) / 15, pos = e.pos;
    if (cnt != e.count)
        TRY(freeslot(p, cnt, &pos));
    u8 b[608], oldset[608];
    TRY(byteio(p, e.pos, oldset, e.count * 32, 0));
    TRY(makeentry(b, name, e.data, e.attr));
    mcopy(b + 6, oldset + 6, 26);
    w16(b + 2, setchecksum(b, cnt * 32));
    TRY(begin());
    r = publish(p, pos, b, cnt);
    if (r < 0)
        return finish(r);
    if (pos != e.pos) {
        for (u32 i = 0; i < e.count; i++)
            oldset[i * 32] &= 0x7f;
        r = byteio(p, e.pos, oldset, e.count * 32, 1);
    }
    return finish(r);
}
int exfat_mount_at(u32 base, u32 partition_sectors) {
    up_ready=up_cache_valid=0;
    strict_walk = 0;
    vol.total_sectors = disk_sectors();
    u8 b[512];
    TRY(disk_read(base,b));
    for (int i = 0; i < 8; i++)
        if (b[3 + i] != "EXFAT   "[i])
            return BAD;
    if (b[510] != 0x55 || b[511] != 0xaa || r16(b + 104) != 0x100 || b[108] != 9 || b[109] > 7 ||
        b[110] != 1)
        return UNSUP;
    if (r32(b + 76) || r32(b + 64) != base || r32(b + 68))
        return UNSUP;
    for (u32 i=11;i<64;i++) if (b[i]) return BAD;
    if (b[112]>100 && b[112]!=255) return BAD;
    vol.base = base;
    vol.sectors = r32(b + 72);
    vol.fat = r32(b + 80);
    u32 fatsz = r32(b + 84);
    vol.heap = r32(b + 88);
    vol.clusters = r32(b + 92);
    vol.root = r32(b + 96);
    vol.spc = 1u << b[109];
    vol.cluster_bytes = vol.spc * 512;
    vol.flags = r16(b + 106);
    if (!vol.sectors || base >= vol.total_sectors || vol.sectors > vol.total_sectors - base || vol.sectors > partition_sectors || base >= 0x10000000u ||
        vol.sectors > 0x10000000u - base || vol.fat < 24 || vol.fat >= vol.sectors || !fatsz ||
        fatsz > vol.sectors - vol.fat || vol.heap < vol.fat + fatsz || vol.heap >= vol.sectors ||
        !vol.clusters || vol.clusters > (vol.sectors - vol.heap) / vol.spc ||
        (vol.clusters + 129u) / 128u > fatsz || !validcl(vol.root))
        return BAD;
    if (vol.flags & 0xfffdu)
        return UNSUP;
    /* Require both boot regions to be intact. Do not silently recover from a
     * bad primary or disagreeing geometry; no automatic repair is performed. */
    u8 primary[512];
    mcopy(primary,b,512);
    u32 sum = 0;
    for (u32 region=0;region<=12;region+=12) {
        sum=0;
        for (u32 sec=0;sec<11;sec++) {
            TRY(rd(region+sec,b));
            if (!sec && region) {
                for (u32 j=0;j<512;j++)
                    if (j!=106 && j!=107 && j!=112 && b[j]!=primary[j]) return BAD;
            }
            for (u32 j=0;j<512;j++) {
                u32 k=sec*512+j;
                if (k!=106 && k!=107 && k!=112) sum=((sum>>1)|(sum<<31))+b[j];
            }
        }
        TRY(rd(region+11,b));
        for (u32 j=0;j<512;j+=4) if(r32(b+j)!=sum) return BAD;
    }
    Stream root = fs_root();
    u32 size;
    TRY(dirsize(root, &size));
    u32 ups = 0;
    int havebm = 0, haveup = 0;
    for (u32 pos = 0; pos <= size && size - pos >= 32; pos += 32) {
        u8 e[32];
        TRY(byteio(root, pos, e, 32, 0));
        if (!e[0])
            break;
        if (e[0] == 0x81 || e[0] == 0x82) {
            if (r32(e + 28))
                return UNSUP;
            Stream s = {r32(e + 20), r32(e + 24), r32(e + 24), 0, 0};
            if (!validcl(s.first))
                return BAD;
            if (e[0] == 0x81) {
                if (e[1] || havebm++)
                    return UNSUP;
                vol.bitmap = s;
            } else {
                if (haveup++)
                    return BAD;
                vol.upcase = s;
                ups = r32(e + 4);
            }
        }
    }
    if (!havebm || !haveup || vol.bitmap.size < (vol.clusters + 7) / 8 || !vol.upcase.size ||
        vol.upcase.size > 131072)
        return BAD;
    sum = 0;
    for (u32 off = 0; off < vol.upcase.size; off += 512) {
        u32 n = vol.upcase.size - off;
        if (n > 512)
            n = 512;
        TRY(byteio(vol.upcase, off, b, n, 0));
        for (u32 j = 0; j < n; j++)
            sum = ((sum >> 1) | (sum << 31)) + b[j];
    }
    if (sum != ups)
        return BAD;
    TRY(up_build());
    vol.mounted = 1;
    return 0;
}

/* Claim every cluster exactly once, and require the allocation bitmap to agree.
 * Last FAT link must be EOC; NoFatChain streams use their declared extent. */
static int audit_chain(Stream stream,uint64_t size) {
    if (!size) return stream.first ? BAD : 0;
    uint64_t needed=1+(size-1)/vol.cluster_bytes;
    if (needed>vol.clusters) return BAD;
    u32 count=(u32)needed,c=stream.first;
    for(u32 i=0;i<count;i++) {
        if(!validcl(c)) return BAD;
        u32 bit=c-2;u8 mask=1u<<(bit&7);
        if((audit_seen[bit/8]&mask) || !(audit_bitmap[bit/8]&mask)) return BAD;
        audit_seen[bit/8]|=mask;
        if(stream.flags&2) { if(i+1<count)c++; }
        else {
            u32 next;TRY(fatget(c,&next));
            if(i+1==count) { if(next<0xfffffff8u)return BAD; }
            else { if(!validcl(next))return BAD; c=next; }
        }
    }
    return 0;
}
static int audit_dir(Stream dir,u32 depth) {
    if(depth>AUDIT_DEPTH) return UNSUP;
    u32 cursor=0;Entry entry;int r;
    while((r=fs_next(dir,&cursor,&entry))>0) {
        #ifdef MULTIOS_FATFS
        if((entry.large&~MULTI_ENTRY_SIZE64) || ((entry.attr&ATTR_DIR)&&entry.large)) return UNSUP;
#else
        if(entry.large) return UNSUP;
#endif
        if(!entry.name_valid || validname(entry.name)<0 || (entry.attr&~0x37u)) return UNSUP;
        TRY(audit_chain(entry.data,entry.size64));
        if(entry.attr&ATTR_DIR) TRY(audit_dir(entry.data,depth+1));
    }
    return r;
}
static int audit_volume(void) {
    if(vol.clusters>AUDIT_CLUSTERS) return UNSUP;
    u32 bytes=(vol.clusters+7)/8,size;
    mzero(audit_seen,sizeof(audit_seen));mzero(audit_bitmap,sizeof(audit_bitmap));
    TRY(byteio(vol.bitmap,0,audit_bitmap,bytes,0));
    Stream root=fs_root();TRY(dirsize(root,&size));
    TRY(audit_chain(root,size));
    TRY(audit_chain(vol.bitmap,vol.bitmap.size));
    TRY(audit_chain(vol.upcase,vol.upcase.size));
    strict_walk=1;audit_budget=AUDIT_RECORDS;
    int r=audit_dir(root,0);strict_walk=0;
    if(r<0)return r;
    /* Extra allocated bits are preserved, not reclaimed: external formatters
     * may reserve clusters not referenced by the supported directory subset.
     * Only referenced-but-free and multiply-owned clusters are unsafe here. */
    return 0;
}

/* Only the descriptor and profile know this backend's concrete entry points. */
static int exfat_probe(const u8 *b){for(u32 i=0;i<8;i++)if(b[3+i]!=(u8)"EXFAT   "[i])return 0;return 1;}
static int exfat_priority(u8 t){return t==7?100:0;}
#ifdef MULTIOS_FATFS
const MultiFsDriver multi_fs_exfat={
 .abi=MULTIOS_FS_ABI,.size=sizeof(MultiFsDriver),.kind=FS_EXFAT,.id="exfat",.name="exFAT",
 .capabilities=MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_EXPERIMENTAL|MULTIOS_FS_RANDOM_IO,
 .probe=exfat_probe,.partition_priority=exfat_priority,.mount=exfat_mount_at,
 .root=exfat_root,.next=multi_fatfs_next,.read=multi_fatfs_read,.read64=multi_fatfs_read64,.name_equal=exfat_equal,.free=multi_fatfs_free,
 .unlock=exfat_unlock,.put=multi_fatfs_put,.remove=multi_fatfs_remove,.rename=multi_fatfs_rename,
 .write_at=multi_fatfs_write_at,.resize=multi_fatfs_resize,
};

#else
const MultiFsDriver multi_fs_exfat={
 .abi=MULTIOS_FS_ABI,.size=sizeof(MultiFsDriver),.kind=FS_EXFAT,.id="exfat",.name="exFAT",
 .capabilities=MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_EXPERIMENTAL,
 .probe=exfat_probe,.partition_priority=exfat_priority,.mount=exfat_mount_at,
 .root=exfat_root,.next=exfat_next,.read=exfat_read,.read64=exfat_read64,.name_equal=exfat_equal,.free=exfat_free,
 .unlock=exfat_unlock,.put=exfat_put,.remove=exfat_remove,.rename=exfat_rename,
};

#endif
