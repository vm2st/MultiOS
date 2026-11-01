/* vm2 studios | t.me/v2vproj — bounded FAT32/VFAT driver for MultiPulti.
 * 512-byte logical sectors; mirrored FATs; ASCII LFN writes. No journal.
 * No repair or automatic dirty-bit clearing. All mutation errors poison mount.
 */
#include "fs_backend.h"
#include "unicode.h"
#define FAT_AUDIT 1048576u
#define FAT_RECORDS 32768u
static u32 fat_size, reserved, fats, active, fsinfo, backup;
static u32 cache_sector = EOC;
static u8 cache[512];
static u8 seen[FAT_AUDIT / 8];
static u32 budget;
static int auditing;
static int validname(const char *n);
static u16 r16(const u8 *p) {
    return p[0] | ((u16)p[1] << 8);
}
static u32 r32(const u8 *p) {
    return r16(p) | ((u32)r16(p + 2) << 16);
}
static void w16(u8 *p, u16 v) {
    p[0] = v;
    p[1] = v >> 8;
}
static void w32(u8 *p, u32 v) {
    w16(p, v);
    w16(p + 2, v >> 16);
}
static int valid(u32 c) {
    return c >= 2 && c - 2 < vol.clusters;
}
static int rd(u32 l, u8 *b) {
    if (l >= vol.sectors)
        return BAD;
    return disk_read(vol.base + l, b);
}
static int wr(u32 l, const u8 *b) {
    cache_sector = EOC;
    if (!vol.writable || vol.poisoned)
        return READONLY;
    if (l >= vol.sectors)
        return BAD;
    return disk_write(vol.base + l, b);
}
static int fatget(u32 c, u32 *n) {
    if (c >= vol.clusters + 2)
        return BAD;
    u32 l = vol.fat + active * fat_size + c / 128;
    if (l != cache_sector) {
        cache_sector = EOC;
        TRY(rd(l, cache));
        cache_sector = l;
    }
    *n = r32(cache + (c % 128) * 4) & 0x0fffffffu;
    return 0;
}
static int fatset(u32 c, u32 n) {
    if (c >= vol.clusters + 2)
        return BAD;
    u8 b[512];
    for (u32 i = 0; i < fats; i++) {
        u32 l = vol.fat + i * fat_size + c / 128;
        TRY(rd(l, b));
        u32 o = (c % 128) * 4;
        w32(b + o, (r32(b + o) & 0xf0000000u) | (n & 0x0fffffffu));
        TRY(wr(l, b));
    }
    return 0;
}
static int nextcl(u32 c, u32 *n) {
    TRY(fatget(c, n));
    if (*n >= 0x0ffffff8u)
        return 0;
    if (!valid(*n))
        return BAD;
    return 1;
}
static int chainsize(u32 c, u32 *sz) {
    *sz = 0;
    for (u32 i = 0; i < vol.clusters; i++) {
        if (!valid(c) || *sz > 0xffffffffu - vol.cluster_bytes)
            return BAD;
        *sz += vol.cluster_bytes;
        u32 n;
        int r = nextcl(c, &n);
        if (r <= 0)
            return r;
        c = n;
    }
    return BAD;
}
static int cluster(u32 c, u32 index, u32 *out) {
    if (index >= vol.clusters || !valid(c))
        return BAD;
    while (index--) {
        u32 n;
        int r = nextcl(c, &n);
        if (r <= 0)
            return r < 0 ? r : BAD;
        c = n;
    }
    *out = c;
    return 0;
}
static int bytes(Stream s, u32 off, u8 *buf, u32 n, int write) {
    while (n) {
        u32 c;
        TRY(cluster(s.first, off / vol.cluster_bytes, &c));
        u32 l = vol.heap + (c - 2) * vol.spc + (off % vol.cluster_bytes) / 512;
        u32 p = off % 512, take = 512 - p;
        if (take > n)
            take = n;
        u8 b[512];
        TRY(rd(l, b));
        if (write) {
            mcopy(b + p, buf, take);
            TRY(wr(l, b));
        } else
            mcopy(buf, b + p, take);
        buf += take;
        off += take;
        n -= take;
    }
    return 0;
}
Stream fat32_root(void) {
    Stream s = {vol.root, 0, 0, ATTR_DIR, 1};
    return s;
}
int fat32_read(Stream s, u32 off, u8 *b, u32 n) {
    if(s.flags&MULTI_STREAM_WIDE)return UNSUP;
    if (s.valid > s.size || off > s.size || n > s.size - off)
        return BAD;
    if (n && !b)
        return INVALID;
    return bytes(s, off, b, n, 0);
}
static u8 checksum(const u8 *s) {
    u8 n = 0;
    for (u32 i = 0; i < 11; i++)
        n = ((n >> 1) | (n << 7)) + s[i];
    return n;
}
static int dot(const u8 *b) {
    return b[0] == '.' && (b[1] == ' ' || (b[1] == '.' && b[2] == ' '));
}
static void shortname(const u8 *b, char *n) {
    u32 k = 0;
    for (u32 j = 0; j < 8 && b[j] != ' '; j++)
        n[k++] = b[j] == 5 ? (char)0xe5 : b[j];
    if (b[8] != ' ') {
        n[k++] = '.';
        for (u32 j = 8; j < 11 && b[j] != ' '; j++)
            n[k++] = b[j];
    }
    n[k] = 0;
}
int fat32_next(Stream d, u32 *cursor, Entry *e) {
    if (!cursor || !e)
        return INVALID;
    u32 size;
    TRY(chainsize(d.first, &size));
    if ((*cursor & 31) || *cursor > size)
        return BAD;
    u16 text[260];
    u32 expect = 0, total = 0, start = 0;
    u8 sum = 0;
    while (*cursor <= size && size - *cursor >= 32) {
        if (auditing) {
            if (!budget)
                return UNSUP;
            budget--;
        }
        u8 b[32];
        u32 pos = *cursor;
        TRY(bytes(d, pos, b, 32, 0));
        *cursor += 32;
        if (!b[0])
            return expect ? BAD : 0;
        if (b[0] == 0xe5) {
            expect = 0;
            total = 0;
            continue;
        }
        if (b[11] == 0x0f) {
            u32 seq = b[0] & 31;
            if (b[0] & 0xa0 || !seq || seq > 20 || b[12] || r16(b + 26))
                return BAD;
            if (b[0] & 0x40) {
                if (expect || total)
                    return BAD;
                total = seq;
                expect = seq;
                sum = b[13];
                start = pos;
                for (u32 j = 0; j < 260; j++)
                    text[j] = 0xffff;
            }
            if (!expect || seq != expect || b[13] != sum)
                return BAD;
            static const u8 offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            for (u32 j = 0; j < 13; j++)
                text[(seq - 1) * 13 + j] = r16(b + offsets[j]);
            expect--;
            continue;
        }
        if (expect)
            return BAD;
        if (b[11] & 8) {
            if (total)
                return BAD;
            continue;
        }
        if (dot(b)) {
            if (total)
                return BAD;
            continue;
        }
        mzero(e, sizeof(*e));
        e->ascii = 1;
        e->pos = total ? start : pos;
        e->count = total + 1;
        e->attr = b[11];
        shortname(b, e->alias);
        if (total) {
            if (checksum(b) != sum)
                return BAD;
            u32 len = 0;
            int ended = 0;
            for (u32 j = 0; j < total * 13; j++) {
                u16 ch = text[j];
                if (!ch) {
                    ended = 1;
                    continue;
                }
                if (ended) {
                    if (ch != 0xffff)
                        return BAD;
                    continue;
                }
                if (ch == 0xffff || len >= 255)
                    return BAD;
                if (ch < 32 || ch > 126)
                    e->ascii = 0;
                len++;
            }
            if (!len)
                return BAD;
            TRY(multi_name_entry(text,len,e));
        } else {
            shortname(b, e->name);
            for (u32 j = 0; e->name[j]; j++)
                if ((u8)e->name[j] < 32 || (u8)e->name[j] > 126)
                    e->ascii = 0;
            /* NT lowercase flags apply only to the base/extension of a short name. */
            int ext = 0;
            for (u32 j = 0; e->name[j]; j++) {
                if (e->name[j] == '.') {
                    ext = 1;
                    continue;
                }
                if (b[12] & (ext ? 16 : 8))
                    if (e->name[j] >= 'A' && e->name[j] <= 'Z')
                        e->name[j] += 32;
            }
        }
#ifdef MULTIOS_FATFS
        if(!total){
            /* Match the configured upstream CP866 SFN driver; aliases stay raw.
             * Decode into the full UTF-8 name, never into the 13-byte alias. */
            extern u16 ff_oem2uni(u16,u16);
            u16 decoded[13];u32 count=0;
            for(u32 j=0;j<11;j++){
                if(j==8&&b[8]!=' ')decoded[count++]='.';
                if(b[j]==' ')continue;
                u8 c=(j==0&&b[j]==5)?0xe5:b[j];
                if(c>='A'&&c<='Z'&&(b[12]&(j<8?8:16)))c+=32;
                u16 ch=ff_oem2uni(c,866);if(!ch)return UNSUP;decoded[count++]=ch;
            }
            TRY(multi_name_entry(decoded,count,e));
        }
#else
        if(!total)e->name_valid=e->ascii; /* OEM-only SFN needs a separate code-page driver. */
#endif
        u32 first = ((u32)r16(b + 20) << 16) | r16(b + 26);
        if (first & 0xf0000000u)
            return BAD;
        e->data = (Stream){first, r32(b + 28), r32(b + 28), 0, 0};
        if (e->attr & ATTR_DIR) {
            if (e->data.size || !valid(first))
                return BAD;
            TRY(chainsize(first, &e->data.size));
            e->data.valid = e->data.size;
            e->data.flags = ATTR_DIR;
        } else if ((e->data.size && !valid(first)) || (!e->data.size && first))
            return BAD;
        return 1;
    }
    return total ? BAD : 0;
}
int fat32_free(u32 *count) {
    if (!count)
        return INVALID;
    *count = 0;
    for (u32 c = 2; c < vol.clusters + 2; c++) {
        u32 n;
        TRY(fatget(c, &n));
        if (!n)
            (*count)++;
    }
    return 0;
}
int fat32_mount_at(u32 base, u32 limit) {
    cache_sector = EOC;
    auditing = 0;
    u8 b[512];
    TRY(disk_read(base, b));
    if (b[510] != 0x55 || b[511] != 0xaa || r16(b + 11) != 512 || !b[13] || b[13] > 128 ||
        (b[13] & (b[13] - 1)) || r16(b + 17) || r16(b + 19) || r16(b + 22) || r16(b + 42))
        return UNSUP;
    reserved = r16(b + 14);
    fats = b[16];
    fat_size = r32(b + 36);
    active = (r16(b + 40) & 128) ? (r16(b + 40) & 15) : 0;
    if (!reserved || !fats || fats > 2 || !fat_size || active >= fats)
        return BAD;
    vol.base = base;
    vol.sectors = r32(b + 32);
    vol.fat = reserved;
    vol.root = r32(b + 44);
    vol.spc = b[13];
    vol.cluster_bytes = vol.spc * 512;
    if (!vol.sectors || vol.sectors > limit || base >= 0x10000000u ||
        vol.sectors > 0x10000000u - base || reserved >= vol.sectors ||
        fat_size > (vol.sectors - reserved) / fats)
        return BAD;
    vol.heap = reserved + fats * fat_size;
    if (vol.heap >= vol.sectors)
        return BAD;
    vol.clusters = (vol.sectors - vol.heap) / vol.spc;
    if (vol.clusters < 65525 || vol.clusters > 0x0fffffedu ||
        fat_size < (vol.clusters + 129u) / 128u || !valid(vol.root))
        return UNSUP;
    fsinfo = r16(b + 48);
    backup = r16(b + 50);
    if (fsinfo == 0xffff)
        fsinfo = 0;
    if (backup == 0xffff)
        backup = 0;
    if ((fsinfo && fsinfo >= reserved) || (backup && backup >= reserved))
        return BAD;
    /* flags: bit 0 records disabled mirroring; bit 1 is a dirty/hard-error volume. */
    vol.flags = (r16(b + 40) & 128) ? 1 : 0;
    u32 n;
    TRY(fatget(0, &n));
    if ((n & 0x0fffff00u) != 0x0fffff00u || (n & 255u) != b[21] || (b[21] != 0xf0 && b[21] < 0xf8))
        return BAD;
    TRY(fatget(1, &n));
    if ((n & 0x0c000000u) != 0x0c000000u)
        vol.flags |= 2;
    u32 size;
    TRY(chainsize(vol.root, &size));
    return 0;
}
static int claim(u32 c, u32 expected, int directory) {
    u32 count = 0;
    for (u32 i = 0; i < vol.clusters; i++) {
        if (!valid(c))
            return BAD;
        u32 bit = c - 2;
        u8 mask = 1u << (bit & 7);
        if (seen[bit / 8] & mask)
            return BAD;
        seen[bit / 8] |= mask;
        count++;
        u32 n;
        int r = nextcl(c, &n);
        if (r < 0)
            return r;
        if (!r)
            return directory || count == expected ? 0 : BAD;
        if (!directory && count >= expected)
            return BAD;
        c = n;
    }
    return BAD;
}
static int auditdir(Stream d, u32 depth, u32 parent) {
    if (depth > 16)
        return UNSUP;
    if (!d.root) {
        u8 dots[64];
        TRY(bytes(d, 0, dots, 64, 0));
        if (dots[0] != '.' || dots[1] != ' ' || dots[32] != '.' || dots[33] != '.' ||
            dots[34] != ' ' || dots[11] != ATTR_DIR || dots[43] != ATTR_DIR)
            return BAD;
        u32 me = ((u32)r16(dots + 20) << 16) | r16(dots + 26),
            up = ((u32)r16(dots + 52) << 16) | r16(dots + 58);
        if (me != d.first || up != parent)
            return BAD;
    }
    u32 p = 0;
    Entry e;
    int r;
    while ((r = fat32_next(d, &p, &e)) > 0) {
        if (!e.name_valid || e.attr & 0xc0)
            return UNSUP;
        TRY(validname(e.name));
        if (!e.data.size) {
            if (e.data.first)
                return BAD;
            continue;
        }
        TRY(claim(e.data.first, 1 + (e.data.size - 1) / vol.cluster_bytes, !!(e.attr & ATTR_DIR)));
        if (e.attr & ATTR_DIR)
            TRY(auditdir(e.data, depth + 1, d.root ? 0 : d.first));
    }
    return r;
}
int fat32_unlock(void) {
    vol.writable = 0;
    if (!vol.mounted)
        return BAD;
    if (!disk_writable())
        return READONLY;
    if (vol.poisoned || vol.flags & 2)
        return BAD;
    if (vol.flags & 1 || vol.clusters > FAT_AUDIT)
        return UNSUP;
    u8 b[512], other[512];
    cache_sector = EOC;
    /* Refuse disagreeing FAT copies and backup geometry instead of repairing. */
    if (backup) {
        TRY(rd(0, b));
        TRY(rd(backup, other));
        for (u32 j = 0; j < 512; j++)
            if (b[j] != other[j])
                return BAD;
    }
    if (fsinfo) {
        TRY(rd(fsinfo, b));
        if (r32(b) != 0x41615252 || r32(b + 484) != 0x61417272 || r32(b + 508) != 0xaa550000)
            return BAD;
        if (backup && fsinfo < reserved - backup) {
            TRY(rd(backup + fsinfo, b));
            if (r32(b) != 0x41615252 || r32(b + 484) != 0x61417272 || r32(b + 508) != 0xaa550000)
                return BAD;
        }
    }
    for (u32 k = 0; k < fats; k++)
        for (u32 j = 0; j < (vol.clusters + 129u) / 128u; j++) {
            TRY(rd(vol.fat + j, b));
            if (k) {
                TRY(rd(vol.fat + k * fat_size + j, other));
                for (u32 i = 0; i < 512; i++)
                    if (b[i] != other[i])
                        return BAD;
            }
        }
    mzero(seen, sizeof(seen));
    budget = FAT_RECORDS;
    TRY(claim(vol.root, 0, 1));
    auditing = 1;
    int r = auditdir(fat32_root(), 0, 0);
    auditing = 0;
    if (r < 0)
        return r;
    vol.writable = 1;
    return 0;
}
static int begin(void) {
    if (!vol.writable || vol.poisoned)
        return READONLY;
    if (vol.flags & 2)
        return BAD;
    u32 n;
    TRY(fatget(1, &n));
    int r = fatset(1, n & ~0x08000000u);
    if (r >= 0)
        r = disk_flush();
    if (r < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
    } else
        vol.flags |= 2;
    return r;
}
static int finish(int r) {
    if (r >= 0 && fsinfo) {
        u8 b[512];
        r = rd(fsinfo, b);
        if (r >= 0) {
            w32(b + 488, EOC);
            w32(b + 492, EOC);
            r = wr(fsinfo, b);
            if (r >= 0 && backup && fsinfo < reserved - backup)
                r = wr(backup + fsinfo, b);
        }
    }
    if (r >= 0)
        r = disk_flush();
    if (r >= 0) {
        u32 n;
        r = fatget(1, &n);
        if (r >= 0)
            r = fatset(1, n | 0x08000000u);
        if (r >= 0)
            r = disk_flush();
    }
    if (r < 0) {
        vol.poisoned = 1;
        vol.writable = 0;
        return r;
    }
    vol.flags &= ~2;
    return 0;
}
static int alloc(u32 count, Stream *s) {
    mzero(s, sizeof(*s));
    u32 prev = 0;
    u8 z[512];
    mzero(z, 512);
    for (u32 c = 2; c < vol.clusters + 2 && count; c++) {
        u32 n;
        TRY(fatget(c, &n));
        if (n)
            continue;
        TRY(fatset(c, 0x0fffffffu));
        if (prev)
            TRY(fatset(prev, c));
        else
            s->first = c;
        for (u32 j = 0; j < vol.spc; j++)
            TRY(wr(vol.heap + (c - 2) * vol.spc + j, z));
        prev = c;
        count--;
        s->size += vol.cluster_bytes;
    }
    s->valid = s->size;
    return count ? NOSPACE : 0;
}
static int release(Stream s) {
    if (!s.first)
        return 0;
    u32 c = s.first;
    for (u32 k = 0; k < vol.clusters; k++) {
        u32 n;
        int r = nextcl(c, &n);
        if (r < 0)
            return r;
        TRY(fatset(c, 0));
        if (!r)
            return 0;
        c = n;
    }
    return BAD;
}
static int validname(const char *n) {return multi_name_valid(n);}
static int alias_used(Stream d, const u8 *alias) {
    u32 p = 0;
    Entry e;
    int r;
    char name[13];
    shortname(alias, name);
    while ((r = fat32_next(d, &p, &e)) > 0)
        if (same(e.alias, name) || (e.ascii && same(e.name, name)))
            return 1;
    return r;
}
static int makealias(Stream d, const char *n, u8 *alias) {
    const char *dotp = 0;
    for (const char *p = n; *p; p++)
        if (*p == '.')
            dotp = p;
    for (u32 serial = 1; serial <= 9999; serial++) {
        for (u32 i = 0; i < 11; i++)
            alias[i] = ' ';
        u32 digits = 1, v = serial;
        while (v >= 10) {
            digits++;
            v /= 10;
        }
        u32 k = 0;
        for (const char *p = n; *p && p != dotp && k < 7 - digits; p++) {
            u8 c = upper(*p);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                alias[k++] = c;
            else if (c != ' ' && c != '.')
                alias[k++] = '_';
        }
        if (!k)
            alias[k++] = '_';
        alias[k++] = '~';
        v = serial;
        for (u32 j = 0; j < digits; j++) {
            alias[k + digits - 1 - j] = '0' + v % 10;
            v /= 10;
        }
        if (dotp) {
            u32 j = 8;
            for (const char *p = dotp + 1; *p && j < 11; p++) {
                u8 c = upper(*p);
                alias[j++] = ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) ? c : '_';
            }
        }
        int r = alias_used(d, alias);
        if (r < 0)
            return r;
        if (!r)
            return 0;
    }
    return NOSPACE;
}
/* Plan extension before setting the dirty bit. Newly allocated tail is zeroed. */
static int slot(Stream d, u32 entries, u32 *pos, u32 *grow) {
    u32 size, run = 0;
    TRY(chainsize(d.first, &size));
    int ended = 0;
    u8 b[32];
    for (u32 p = 0; p < size; p += 32) {
        TRY(bytes(d, p, b, 32, 0));
        if (!b[0])
            ended = 1;
        if (ended || b[0] == 0xe5)
            run++;
        else
            run = 0;
        if (run == entries) {
            *pos = p + 32 - entries * 32;
            *grow = 0;
            return 0;
        }
    }
    *pos = size - run * 32;
    u32 need = (entries - run) * 32;
    *grow = 1 + (need - 1) / vol.cluster_bytes;
    if (size > 0xffffffffu - (*grow) * vol.cluster_bytes)
        return UNSUP;
    return 0;
}
static int extend(Stream d, u32 grow) {
    if (!grow)
        return 0;
    u32 size, last;
    TRY(chainsize(d.first, &size));
    TRY(cluster(d.first, size / vol.cluster_bytes - 1, &last));
    Stream tail;
    TRY(alloc(grow, &tail));
    TRY(disk_flush());
    return fatset(last, tail.first);
}
static void sfn_data(u8 *b, Stream s, u16 attr) {
    b[11] = attr;
    b[12] = 0;
    w16(b + 20, s.first >> 16);
    w16(b + 26, s.first);
    w32(b + 28, (attr & ATTR_DIR) ? 0 : s.size);
    w16(b + 16, 0x21);
    w16(b + 18, 0x21);
    w16(b + 24, 0x21);
}
static void entryset(u8 *b, const char *name, const u8 *alias, Stream s, u16 attr) {
    u16 text[255];u32 len=0;
    (void)multi_utf8_to16(name,text,255,&len); /* caller validated before starting transaction */
    u32 lfn=(len+12)/13;
    mzero(b, (lfn + 1) * 32);
    u8 sum = checksum(alias);
    static const u8 offsets[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
    for (u32 i = 0; i < lfn; i++) {
        u32 seq = lfn - i;
        u8 *a = b + i * 32;
        a[0] = seq | (i ? 0 : 0x40);
        a[11] = 0x0f;
        a[13] = sum;
        for (u32 j = 0; j < 13; j++) {
            u32 at = (seq - 1) * 13 + j;
            w16(a + offsets[j], at < len ? text[at] : at == len ? 0 : 0xffff);
        }
    }
    mcopy(b + lfn * 32, alias, 11);
    sfn_data(b + lfn * 32, s, attr);
}
static int publish(Stream d, u32 pos, u8 *b, u32 count) {
    u32 size;
    TRY(chainsize(d.first, &size));
    int ended = 0;
    u8 e[32];
    for (u32 i = 0; i < count; i++) {
        TRY(bytes(d, pos + i * 32, e, 32, 0));
        if (!e[0])
            ended = 1;
    }
    if (ended && pos + count * 32 < size) {
        mzero(e, 32);
        TRY(bytes(d, pos + count * 32, e, 32, 1));
    }
    if (count > 1)
        TRY(bytes(d, pos, b, (count - 1) * 32, 1));
    TRY(disk_flush());
    return bytes(d, pos + (count - 1) * 32, b + (count - 1) * 32, 32, 1);
}
int fat32_put(const char *path, const u8 *text, u32 len, int mode, const char *source) {
    if (mode < 0 || mode > 4 || (len && !text && mode != 3 && mode != 4) || (mode == 4 && !source))
        return INVALID;
    if (!vol.writable || vol.poisoned)
        return READONLY;
    if (mode == 3) {
        u32 depth = 0;
        for (const char *p = path; *p; p++)
            if (*p == '/')
                depth++;
        if (depth > 16)
            return UNSUP;
    }
    Stream parent, input = {0};
    char name[256];
    Entry old, src;
    TRY(fs_parent(path, &parent, name));
    TRY(validname(name));
    int r = fs_lookup(parent, name, &old), found = r == 0;
    if (r < 0 && r != NOTFOUND)
        return r;
    if (found && (mode == 0 || mode == 3 || mode == 4))
        return EXISTS;
    if (found && old.attr & ATTR_DIR)
        return ISDIR;
    if (found && old.attr & 1)
        return READONLY;
    u32 keep = 0, total = len;
    if (mode == 2 && found) {
        input = old.data;
        keep = input.size;
        if (len > 0xffffffffu - keep)
            return UNSUP;
        total = keep + len;
    }
    if (mode == 4) {
        Stream p;
        int root;
        TRY(fs_resolve(source, &p, &src, &root));
        if (src.attr & ATTR_DIR)
            return ISDIR;
        input = src.data;
        keep = total = input.size;
    }
    if (mode == 3)
        total = vol.cluster_bytes;
    if (total > 0xffff0000u)
        return UNSUP;
    u32 count = 1 + (multi_name_units(name) + 12) / 13, pos = 0, grow = 0,
        required = total ? 1 + (total - 1) / vol.cluster_bytes : 0;
    u8 set[672], alias[11];
    if (found) {
        pos = old.pos;
        count = old.count;
        TRY(bytes(parent, pos + (count - 1) * 32, alias, 11, 0));
    } else {
        TRY(makealias(parent, name, alias));
        TRY(slot(parent, count, &pos, &grow));
    }
    u32 free;
    TRY(fat32_free(&free));
    if (required > free || grow > free - required)
        return NOSPACE;
    TRY(begin());
    r = extend(parent, grow);
    if (r < 0)
        return finish(r);
    Stream news;
    r = alloc(required, &news);
    if (r < 0)
        return finish(r);
    u8 b[512];
    for (u32 off = 0; off < keep;) {
        u32 n = keep - off;
        if (n > 512)
            n = 512;
        r = fat32_read(input, off, b, n);
        if (r < 0)
            return finish(r);
        r = bytes(news, off, b, n, 1);
        if (r < 0)
            return finish(r);
        off += n;
    }
    if (mode != 3 && mode != 4 && len) {
        r = bytes(news, keep, (u8 *)text, len, 1);
        if (r < 0)
            return finish(r);
    }
    news.size = total;
    news.valid = total;
    if (mode == 3) {
        u8 dots[64];
        mzero(dots, 64);
        for (u32 i = 0; i < 11; i++)
            dots[i] = dots[i + 32] = ' ';
        dots[0] = '.';
        dots[32] = dots[33] = '.';
        sfn_data(dots, news, ATTR_DIR);
        Stream up = {parent.root ? 0 : parent.first, 0, 0, 0, 0};
        sfn_data(dots + 32, up, ATTR_DIR);
        r = bytes(news, 0, dots, 64, 1);
        if (r < 0)
            return finish(r);
    }
    if (found) {
        r = bytes(parent, pos + (count - 1) * 32, set, 32, 0);
        if (r < 0)
            return finish(r);
        sfn_data(set, news, old.attr | 32);
        r = disk_flush();
        if (r >= 0)
            r = bytes(parent, pos + (count - 1) * 32, set, 32, 1);
    } else {
        entryset(set, name, alias, news, mode == 3 ? ATTR_DIR : 32);
        r = publish(parent, pos, set, count);
    }
    if (r < 0)
        return finish(r);
    if (found) {
        r = release(old.data);
        if (r < 0)
            return finish(r);
    }
    return finish(0);
}
static int erase(Stream p, Entry *e) {
    u8 b[32];
    for (u32 i = 0; i < e->count; i++) {
        TRY(bytes(p, e->pos + i * 32, b, 32, 0));
        b[0] = 0xe5;
        TRY(bytes(p, e->pos + i * 32, b, 32, 1));
    }
    return 0;
}
int fat32_remove(const char *path, int dir) {
    if (!vol.writable || vol.poisoned)
        return READONLY;
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
        Entry child;
        u32 off = 0;
        int r = fat32_next(e.data, &off, &child);
        if (r < 0)
            return r;
        if (r)
            return NOTEMPTY;
    }
    TRY(begin());
    int r = erase(p, &e);
    if (r >= 0)
        r = disk_flush();
    if (r >= 0)
        r = release(e.data);
    return finish(r);
}
int fat32_rename(const char *path, const char *name) {
    if (!vol.writable || vol.poisoned)
        return READONLY;
    TRY(validname(name));
    Stream p;
    Entry e, other;
    int root;
    TRY(fs_resolve(path, &p, &e, &root));
    if (root)
        return INVALID;
    if (e.attr & 1)
        return READONLY;
    int r = fs_lookup(p, name, &other);
    if (r < 0 && r != NOTFOUND)
        return r;
    if (!r && other.pos != e.pos)
        return EXISTS;
    if (e.name_valid && streq(e.name, name))
        return 0;
    u8 alias[11];
    TRY(makealias(p, name, alias));
    u32 count = 1 + (multi_name_units(name) + 12) / 13, pos, grow;
    TRY(slot(p, count, &pos, &grow));
    u32 free;
    TRY(fat32_free(&free));
    if (grow > free)
        return NOSPACE;
    u8 b[672];
    entryset(b, name, alias, e.data, e.attr);
    TRY(begin());
    r = extend(p, grow);
    if (r >= 0)
        r = publish(p, pos, b, count);
    if (r >= 0)
        r = erase(p, &e);
    return finish(r);
}

/* Only the descriptor and profile know this backend's concrete entry points. */
int fat32_read64(const MultiFile64 *f,uint64_t off,u8 *b,u32 n){
    if(!f||(n&&!b))return INVALID;
    if(f->size>UINT32_MAX||f->valid!=f->size||f->data.size!=f->size||f->data.valid!=f->valid)return BAD;
    if(off>f->size||(uint64_t)n>f->size-off)return BAD;
    return fat32_read(f->data,(u32)off,b,n);
}
static int fat32_probe(const u8 *b) {
    return (b[0] == 0xeb || b[0] == 0xe9) && b[11] == 0 && b[12] == 2 && (b[16] == 1 || b[16] == 2) && !b[17] && !b[18] &&
           !b[22] && !b[23] && (b[36] || b[37] || b[38] || b[39]);
}
static int fat32_priority(u8 t) {
    return t == 0x0b || t == 0x0c || t == 0x1b || t == 0x1c ? 90 : t == 0xef ? 10 : 0;
}
#ifdef MULTIOS_FATFS
const MultiFsDriver multi_fs_fat32 = {
    .abi = MULTIOS_FS_ABI,
    .size = sizeof(MultiFsDriver),
    .kind = FS_FAT32,
    .id = "fat32",
    .name = "FAT32",
    .capabilities = MULTIOS_FS_READ | MULTIOS_FS_WRITE | MULTIOS_FS_EXPERIMENTAL|MULTIOS_FS_RANDOM_IO,
    .probe = fat32_probe,
    .partition_priority = fat32_priority,
    .mount = fat32_mount_at,
    .root = fat32_root,
    .next = multi_fatfs_next,
    .read = multi_fatfs_read,
    .read64 = multi_fatfs_read64,
    .name_equal = multi_default_equal,
    .free = multi_fatfs_free,
    .unlock = fat32_unlock,
    .put = multi_fatfs_put,
    .remove = multi_fatfs_remove,
    .rename = multi_fatfs_rename,
 .write_at=multi_fatfs_write_at,.resize=multi_fatfs_resize,
};

#else
const MultiFsDriver multi_fs_fat32 = {
    .abi = MULTIOS_FS_ABI,
    .size = sizeof(MultiFsDriver),
    .kind = FS_FAT32,
    .id = "fat32",
    .name = "FAT32",
    .capabilities = MULTIOS_FS_READ | MULTIOS_FS_WRITE | MULTIOS_FS_EXPERIMENTAL,
    .probe = fat32_probe,
    .partition_priority = fat32_priority,
    .mount = fat32_mount_at,
    .root = fat32_root,
    .next = fat32_next,
    .read = fat32_read,
    .read64 = fat32_read64,
    .name_equal = multi_default_equal,
    .free = fat32_free,
    .unlock = fat32_unlock,
    .put = fat32_put,
    .remove = fat32_remove,
    .rename = fat32_rename,
};

#endif
