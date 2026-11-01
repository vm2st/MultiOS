/* vm2 studios | t.me/v2vproj — registry-driven VFS; no filesystem-specific dispatch. */
#include "fs_backend.h"
#include "unicode.h"
#ifdef MULTIOS_STORAGE_TX
#include "multios/storage_tx.h"
#endif
#ifndef MULTIOS_FS_REGISTRY
#define MULTIOS_FS_REGISTRY "../profiles/filesystems.registry.h"
#endif
static const MultiFsDriver *const drivers[] = {
#include MULTIOS_FS_REGISTRY
};
#define DRIVER_COUNT (sizeof(drivers) / sizeof(drivers[0]))
Volume vol;
static u32 mount_epoch;
/* Source views cannot survive remounts or metadata-changing operations. */
static int invalidate_views(void){
    if(mount_epoch==UINT32_MAX){vol.mounted=0;vol.writable=0;return UNSUP;}
    mount_epoch++;return 0;
}
static u32 r32(const u8 *p) {
    return p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static int id_ok(const char *s) {
    if (!s || !*s)
        return 0;
    u32 n = 0;
    for (; s[n] && n < 31; n++)
        if (!((s[n] >= 'a' && s[n] <= 'z') || (s[n] >= '0' && s[n] <= '9') || s[n] == '-'))
            return 0;
    return n < 31;
}
int multi_fs_validate(const MultiFsDriver *const *r, u32 n) {
    if (!r || !n || n > 16)
        return INVALID;
    for (u32 i = 0; i < n; i++) {
        const MultiFsDriver *d = r[i];
        if (!d || d->abi != MULTIOS_FS_ABI || d->size != sizeof(*d) || d->kind < 0 ||
            !id_ok(d->id) || !d->name || !d->name[0] || !(d->capabilities & MULTIOS_FS_READ) ||
            d->capabilities & ~63u || !d->probe || !d->partition_priority || !d->mount || !d->root ||
            !d->next || !d->read || !d->read64 || !d->name_equal || !d->free)
            return INVALID;
        if ((d->capabilities & MULTIOS_FS_WRITE) &&
            (!d->unlock || !d->put || !d->remove || !d->rename))
            return INVALID;
        if ((d->capabilities & MULTIOS_FS_RANDOM_IO) &&
            (!(d->capabilities & MULTIOS_FS_WRITE) || !d->write_at || !d->resize)) return INVALID;
        if (!(d->capabilities & MULTIOS_FS_RANDOM_IO) && (d->write_at || d->resize)) return INVALID;
        if(d->capabilities&MULTIOS_FS_NAMED_STREAMS){
            if(!(d->capabilities&MULTIOS_FS_WRITE)||!d->stream_next||!d->stream_read||!d->stream_change)return INVALID;
        }else if(d->stream_next||d->stream_read||d->stream_change)return INVALID;
        if(d->capabilities&MULTIOS_FS_HARDLINKS){if(!(d->capabilities&MULTIOS_FS_WRITE)||!d->link)return INVALID;}
        else if(d->link)return INVALID;
        for (u32 j = 0; j < i; j++)
            if (r[j]->kind == d->kind || streq(r[j]->id, d->id))
                return INVALID;
    }
    return 0;
}
static const MultiFsDriver *current(void) {
    for (u32 i = 0; i < DRIVER_COUNT; i++)
        if (drivers[i]->kind == vol.kind)
            return drivers[i];
    return 0;
}
static int identify(const u8 *b, const MultiFsDriver **found) {
    *found = 0;
    for (u32 i = 0; i < DRIVER_COUNT; i++) {
        int r = drivers[i]->probe(b);
        if (r < 0)
            return r;
        if (r > 1)
            return BAD;
        if (r) {
            if (*found)
                return BAD;
            *found = drivers[i];
        }
    }
    return *found ? 0 : UNSUP;
}
const char *fs_name(void) {
    const MultiFsDriver *d = current();
    return d ? d->name : "unavailable";
}
u32 fs_capabilities(void) {
    const MultiFsDriver *d = current();
    return d ? d->capabilities : 0;
}
void fs_list_drivers(void) {
#ifdef MULTIOS_STORAGE_TX
    out("Transactional storage profile: prepared image required; per-image undo budget; check driver capabilities\n");
    out(multi_storage_description());out("\n");
#endif
    out("Filesystem module ABI ");
    number(MULTIOS_FS_ABI);
    out(" (static registry)\n");
    for (u32 i = 0; i < DRIVER_COUNT; i++) {
        const MultiFsDriver *d = drivers[i];
        out(d->id);
        out(" | ");
        out(d->name);
        out((d->capabilities & MULTIOS_FS_WRITE) ? " | experimental RW"
                                                 : " | read-only; no write callbacks");
        out("\n");
    }
}
int fs_mount_select(int selection) {
    TRY(invalidate_views());
    if (selection < 0 || selection > 5)
        return INVALID;
    TRY(multi_fs_validate(drivers, DRIVER_COUNT));
    if (vol.mounted) {
        TRY(disk_flush());
        if (vol.poisoned)
            return BAD;
    }
#ifdef MULTIOS_STORAGE_TX
    int prepared=multi_storage_prepare();
    if(prepared<0){vol.mounted=0;vol.writable=0;vol.poisoned=1;vol.kind=-1;return prepared;}
#endif
    mzero(&vol, sizeof(vol));
    vol.kind = -1;
    vol.total_sectors = disk_sectors();
    u8 b[512], mbr[512];
    const MultiFsDriver *driver = 0;
    u32 base = 0, limit = vol.total_sectors;
    TRY(disk_read(0, b));
    mcopy(mbr, b, 512);
    int detected = identify(b, &driver);
    if (detected < 0 && detected != UNSUP)
        return detected;
    if ((selection >= 1 && selection <= 4) || (selection == 0 && !driver)) {
        if (b[510] != 0x55 || b[511] != 0xaa)
            return BAD;
        int chosen = -1, best = 0;
        if (selection)
            chosen = selection - 1;
        else
            for (int i = 0; i < 4; i++)
                for (u32 j = 0; j < DRIVER_COUNT; j++) {
                    int score = drivers[j]->partition_priority(mbr[446 + i * 16 + 4]);
                    if (score < 0)
                        return INVALID;
                    if (score > best) {
                        best = score;
                        chosen = i;
                    }
                }
        if (chosen < 0)
            return UNSUP;
        const u8 *p = mbr + 446 + chosen * 16;
        base = r32(p + 8);
        limit = r32(p + 12);
        if (!p[4] || !base || !limit || base >= vol.total_sectors ||
            limit > vol.total_sectors - base)
            return BAD;
        TRY(disk_read(base, b));
        TRY(identify(b, &driver));
    }
    if (!driver)
        return UNSUP;
    vol.kind = driver->kind;
    vol.base = base;
    vol.sectors = limit;
    int r = driver->mount(base, limit);
    if (r < 0) {
        vol.mounted = 0;
        vol.writable = 0;
        return r;
    }
    vol.mounted = 1;
    vol.writable = 0;
    return 0;
}
int fs_mount(void) {
    return fs_mount_select(0);
}
Stream fs_root(void) {
    const MultiFsDriver *d = current();
    Stream zero = {0};
    return d ? d->root() : zero;
}
int fs_next(Stream s, u32 *c, Entry *e) {
    if (!c || !e)
        return INVALID;
    const MultiFsDriver *d = current();
    if(!d)return UNSUP;
    int r=d->next(s,c,e);
    if(r>0&&!e->wide){e->size64=e->data.size;e->valid64=e->data.valid;e->wide=1;}
    return r;
}
int fs_read(Stream s, u32 o, u8 *b, u32 n) {
    if(s.flags&MULTI_STREAM_WIDE)return UNSUP;
    if (n && !b)
        return INVALID;
    const MultiFsDriver *d = current();
    return d ? d->read(s, o, b, n) : UNSUP;
}
int fs_free(u32 *n) {
    if (!n)
        return INVALID;
    const MultiFsDriver *d = current();
    return d ? d->free(n) : UNSUP;
}
int fs_names_equal(const char *a, const char *b) {
    if (!a || !b) return INVALID;
    const MultiFsDriver *d = current();
    return d ? d->name_equal(a, b) : UNSUP;
}
static int write_allowed(void) {
    return vol.mounted && vol.writable && !vol.poisoned && (fs_capabilities() & MULTIOS_FS_WRITE);
}
int fs_unlock(void) {
    const MultiFsDriver *d = current();
    vol.writable = 0;
    if (!vol.mounted)
        return BAD;
    if (vol.poisoned)
        return BAD;
    if (!d || !(d->capabilities & MULTIOS_FS_WRITE) || !d->unlock)
        return READONLY;
#ifdef MULTIOS_STORAGE_TX
    if(!multi_storage_matches(vol.base,vol.sectors))return READONLY;
#endif
    return d->unlock();
}
int fs_put(const char *p, const u8 *b, u32 n, int m, const char *s) {
    if (!p || m < 0 || m > 4 || (n && !b && m != 3 && m != 4) || (m == 4 && !s))
        return INVALID;
    if (!write_allowed())
        return READONLY;
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());
    return multi_storage_finish(current()->put(p, b, n, m, s));
#else
    return current()->put(p, b, n, m, s);
#endif
}
int fs_write64(const char *path, uint64_t off, const u8 *data, u32 n) {
    if (!path || (n && !data)) return INVALID;
    if (!write_allowed()) return READONLY;
    const MultiFsDriver *d=current();
    if (!(d->capabilities & MULTIOS_FS_RANDOM_IO)) return UNSUP;
    MultiFile64 f;TRY(fs_open64(path,&f));
    if (off>f.size || (uint64_t)n>f.size-off) return INVALID;
    if (!n) return 0;
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());return multi_storage_finish(d->write_at(path,off,data,n));
#else
    return d->write_at(path,off,data,n);
#endif
}
int fs_resize64(const char *path, uint64_t size) {
    if (!path) return INVALID;
    if (!write_allowed()) return READONLY;
    const MultiFsDriver *d=current();
    if (!(d->capabilities & MULTIOS_FS_RANDOM_IO)) return UNSUP;
    if (size>INT64_MAX || (vol.kind==FS_FAT32 && size>UINT32_MAX)) return UNSUP;
    MultiFile64 f;TRY(fs_open64(path,&f));if(f.size==size)return 0;
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());return multi_storage_finish(d->resize(path,size));
#else
    return d->resize(path,size);
#endif
}
static int stream_name_valid(const char *name){
    if(!name)return INVALID;return multi_stream_name_valid(name);
}
int fs_stream_next(const char *path,u32 *cursor,MultiNamedStream *info){
    if(!path||!cursor||!info)return INVALID;if(!vol.mounted)return BAD;
    const MultiFsDriver*d=current();if(!d||!(d->capabilities&MULTIOS_FS_NAMED_STREAMS))return UNSUP;
    MultiFile64 f;TRY(fs_open64(path,&f));return d->stream_next(path,cursor,info);
}
int fs_stream_read(const char *path,const char *name,uint64_t off,u8 *b,u32 n){
    if(!path||(n&&!b))return INVALID;TRY(stream_name_valid(name));if(!vol.mounted)return BAD;
    const MultiFsDriver*d=current();if(!d||!(d->capabilities&MULTIOS_FS_NAMED_STREAMS))return UNSUP;
    MultiFile64 f;TRY(fs_open64(path,&f));return d->stream_read(path,name,off,b,n);
}
int fs_stream_change(const char *path,const char *name,int op,uint64_t off,const u8*b,u32 n){
    if(!path||op<MULTIOS_STREAM_CREATE||op>MULTIOS_STREAM_REMOVE||(n&&!b))return INVALID;
    TRY(stream_name_valid(name));
    if((op!=MULTIOS_STREAM_WRITE&&op!=MULTIOS_STREAM_RESIZE&&off)||
       ((op==MULTIOS_STREAM_RESIZE||op==MULTIOS_STREAM_REMOVE)&&n)||off>INT64_MAX)return INVALID;
    if(!write_allowed())return READONLY;const MultiFsDriver*d=current();
    if(!(d->capabilities&MULTIOS_FS_NAMED_STREAMS))return UNSUP;
    MultiFile64 f;TRY(fs_open64(path,&f));TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());return multi_storage_finish(d->stream_change(path,name,op,off,b,n));
#else
    return d->stream_change(path,name,op,off,b,n);
#endif
}
int fs_link(const char *source,const char *newpath){
    if(!source||!newpath)return INVALID;if(!write_allowed())return READONLY;
    const MultiFsDriver*d=current();if(!(d->capabilities&MULTIOS_FS_HARDLINKS))return UNSUP;
    MultiFile64 f;TRY(fs_open64(source,&f));Stream parent;char name[PATHMAX];TRY(fs_parent(newpath,&parent,name));TRY(multi_name_valid(name));
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());return multi_storage_finish(d->link(source,newpath));
#else
    return d->link(source,newpath);
#endif
}
int fs_remove(const char *p, int d) {
    if (!p)
        return INVALID;
    if (!write_allowed())
        return READONLY;
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());
    return multi_storage_finish(current()->remove(p, d));
#else
    return current()->remove(p, d);
#endif
}
int fs_rename(const char *p, const char *n) {
    if (!p || !n)
        return INVALID;
    if (!write_allowed())
        return READONLY;
    TRY(invalidate_views());
#ifdef MULTIOS_STORAGE_TX
    TRY(multi_storage_begin());
    return multi_storage_finish(current()->rename(p, n));
#else
    return current()->rename(p, n);
#endif
}
static int lookup(Stream d, const char *n, Entry *e, int wide) {
    if (!n || !e)
        return INVALID;
    u32 p = 0;
    int r;
    u32 units;TRY(multi_utf8_to16(n,0,255,&units));
    while ((r = fs_next(d, &p, e)) > 0) {
        int equal=0;
        if(e->name_valid||e->ascii){equal=current()->name_equal(e->name,n);if(equal<0)return equal;}
        if(equal || (e->alias[0] && same(e->alias,n)))
            return (e->large && (!wide || (e->large&~MULTI_ENTRY_SIZE64) || (e->attr&ATTR_DIR))) ? UNSUP : 0;
    }
    return r < 0 ? r : NOTFOUND;
}
int fs_lookup(Stream d,const char *n,Entry *e){return lookup(d,n,e,0);}
/* Paths passed by shell are absolute and normalized. */
static int resolve(const char *path, Stream *parent, Entry *e, int *isroot, int wide) {
    if (!path || !parent || !e || !isroot)
        return INVALID;
    Stream d = fs_root();
    char part[256];
    if (path[0] != '/')
        return INVALID;
    path++;
    *isroot = 0;
    mzero(e, sizeof(*e));
    if (!*path) {
        e->attr = ATTR_DIR;
        e->data = d;
        *parent = d;
        *isroot = 1;
        return 0;
    }
    while (*path) {
        u32 n = 0;
        while (*path && *path != '/') {
            if (n >= 255)
                return TOOLONG;
            part[n++] = *path++;
        }
        part[n] = 0;
        if (!n)
            return INVALID;
        TRY(lookup(d, part, e,wide));
        *parent = d;
        if (!*path)
            return 0;
        if (!(e->attr & ATTR_DIR))
            return NOTDIR;
        d = e->data;
        path++;
    }
    return INVALID;
}
int fs_resolve(const char *path,Stream *parent,Entry *e,int *isroot){return resolve(path,parent,e,isroot,0);}
int fs_open64(const char *path,MultiFile64 *f){
    if(!path||!f)return INVALID;
    if(!vol.mounted)return BAD;
    Stream parent;Entry e;int root;
    TRY(resolve(path,&parent,&e,&root,1));
    if(e.attr&ATTR_DIR)return ISDIR;
    if(e.valid64>e.size64)return BAD;
    MultiFile64 value={.data=e.data,.size=e.size64,.valid=e.valid64,.epoch=mount_epoch,.kind=vol.kind};
    *f=value;return 0;
}
int fs_read64(const MultiFile64 *f,uint64_t off,u8 *b,u32 n){
    if(!f||(n&&!b))return INVALID;
    if(!vol.mounted||f->epoch!=mount_epoch||f->kind!=vol.kind)return BAD;
    if(f->valid>f->size||off>f->size||(uint64_t)n>f->size-off)return BAD;
    const MultiFsDriver *d=current();return d?d->read64(f,off,b,n):UNSUP;
}
int fs_parent(const char *path, Stream *parent, char *name) {
    if (!path || !parent || !name)
        return INVALID;
    char p[256];
    u32 n = slen(path), i = n;
    if (n >= 256 || n < 2 || path[0] != '/')
        return INVALID;
    while (i && path[i - 1] != '/')
        i--;
    mcopy(name, path + i, n - i + 1);
    if (i == 1) {
        *parent = fs_root();
        return 0;
    }
    mcopy(p, path, i - 1);
    p[i - 1] = 0;
    Entry e;
    Stream par;
    int root;
    TRY(fs_resolve(p, &par, &e, &root));
    if (!(e.attr & ATTR_DIR))
        return NOTDIR;
    *parent = e.data;
    return 0;
}
