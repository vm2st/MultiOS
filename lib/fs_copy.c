/* vm2 studios | t.me/v2vproj — MultiPulti | MIT */
#include "multios/copy.h"
#include "unicode.h"
#include "multios/fs_driver.h"
#ifdef MULTIOS_STORAGE_TX
static u8 copy_buffer[MULTIOS_COPY_CHUNK], compare_buffer[512];
static int busy;
static u32 next_name;
static int bytes_equal(const u8 *a, const u8 *b, u32 n) {
    while (n--) if (*a++ != *b++) return 0;
    return 1;
}
static int path_length(const char *s, u32 *length, u32 *slash) {
    if (!s || s[0] != '/') return INVALID;
    u32 n = 0, last = 0;
    while (n < PATHMAX && s[n]) {
        if (s[n] == '/') last = n + 1;
        n++;
    }
    if (n == PATHMAX) return TOOLONG;
    if (n < 2 || last == n) return INVALID;
    /* Reject noncanonical paths rather than guessing . / .. semantics. */
    for (u32 i = 1, start = 1; i <= n; i++) {
        if (s[i] != '/' && i != n) continue;
        u32 size = i - start;
        if (!size || (size == 1 && s[start] == '.') ||
            (size == 2 && s[start] == '.' && s[start + 1] == '.')) return INVALID;
        start = i + 1;
    }
    *length = n; *slash = last; return 0;
}
static int absent(const char *path) {
    MultiFile64 f;
    int r = fs_open64(path, &f);
    return r == NOTFOUND ? 0 : r == 0 || r == ISDIR ? EXISTS : r;
}
static int source_view(const char *source, const MultiFile64 *identity, MultiFile64 *f) {
    TRY(fs_open64(source, f));
    if (f->size != identity->size || f->valid != identity->valid ||
        f->data.first != identity->data.first || f->kind != identity->kind) return BAD;
    return 0;
}
/* Empty FAT files legitimately have cluster 0. Use directory-entry identity
 * too: otherwise resuming through a case/8.3 alias could rename the SOURCE. */
static int empty_alias(const char *source, const char *stage) {
    Stream a, b; Entry x, y; int ar, br;
    TRY(fs_resolve(source, &a, &x, &ar));
    TRY(fs_resolve(stage, &b, &y, &br));
    return a.first == b.first && a.root == b.root && x.pos == y.pos;
}
static int progress(MultiCopyProgress fn, void *ctx, uint64_t off, uint64_t n, int phase) {
    return fn && fn(ctx, off, n, phase) ? CANCELLED : 0;
}
/* Fresh views are acquired after every mutation: VFS epochs invalidate old ones. */
static int compare_prefix(const char *source, const MultiFile64 *identity,
                          const char *stage, uint64_t count, int phase,
                          MultiCopyProgress fn, void *ctx) {
    MultiFile64 a, b;
    TRY(source_view(source, identity, &a));
    TRY(fs_open64(stage, &b));
    if (b.size != count || b.size > a.size) return BAD;
    for (uint64_t off = 0; off < count;) {
        TRY(progress(fn, ctx, off, count, phase));
        u32 n = count - off > sizeof copy_buffer ? sizeof copy_buffer : (u32)(count - off);
        TRY(fs_read64(&a, off, copy_buffer, n));
        for (u32 i = 0; i < n;) {
            u32 k = n - i > sizeof compare_buffer ? sizeof compare_buffer : n - i;
            TRY(fs_read64(&b, off + i, compare_buffer, k));
            if (!bytes_equal(copy_buffer + i, compare_buffer, k)) return BAD;
            i += k;
        }
        off += n;
    }
    return progress(fn, ctx, count, count, phase);
}
/* ADS inventory is path-based and restarted after mutation: no stale cursor
 * or file views. The HAL is single-writer; callbacks cannot mutate the VFS. */
static int find_stream(const char*path,const char*name,MultiNamedStream*out){
 u32 cursor=0;MultiNamedStream info;int r,found=0;
 while((r=fs_stream_next(path,&cursor,&info))>0){int equal=fs_names_equal(info.name,name);if(equal<0)return equal;if(equal){if(found)return BAD;*out=info;found=1;}}
 return r<0?r:found?0:NOTFOUND;
}
static int compare_stream(const char*source,const char*stage,const char*name,uint64_t count,MultiCopyProgress fn,void*ctx){
 for(uint64_t off=0;off<count;){
  TRY(progress(fn,ctx,off,count,MULTIOS_COPY_STREAM_VERIFY));
  u32 n=count-off>sizeof copy_buffer?sizeof copy_buffer:(u32)(count-off);
  TRY(fs_stream_read(source,name,off,copy_buffer,n));
  for(u32 i=0;i<n;){u32 k=n-i>sizeof compare_buffer?sizeof compare_buffer:n-i;
   TRY(fs_stream_read(stage,name,off+i,compare_buffer,k));if(!bytes_equal(copy_buffer+i,compare_buffer,k))return BAD;i+=k;}
  off+=n;
 }return 0;
}
static int copy_named_streams(const char*source,const char*stage,MultiCopyProgress fn,void*ctx){
 u32 cursor=0;MultiNamedStream src,dst;int r;
 /* Case-sensitive NTFS can contain two ADS that our case-folded API cannot
  * distinguish. Refuse the whole inventory, even when their bytes match. */
 while((r=fs_stream_next(source,&cursor,&src))>0){
  if(src.flags&0x4000u)return UNSUP;
  TRY(find_stream(source,src.name,&dst));
 }if(r<0)return r;
 cursor=0;
 /* Never change an unrecognized staging stream or a mismatching prefix. */
 while((r=fs_stream_next(stage,&cursor,&dst))>0){
  int found=find_stream(source,dst.name,&src);if(found==NOTFOUND)return BAD;TRY(found);
  if(dst.size>src.size)return BAD;TRY(compare_stream(source,stage,dst.name,dst.size,fn,ctx));
 }if(r<0)return r;
 cursor=0;
 while((r=fs_stream_next(source,&cursor,&src))>0){
  if(src.flags&0x4000u)return UNSUP; /* encrypted named DATA is not plaintext */
  int found=find_stream(stage,src.name,&dst);uint64_t off=0;
  if(found==NOTFOUND){TRY(progress(fn,ctx,0,src.size,MULTIOS_COPY_STREAM_DATA));TRY(fs_stream_change(stage,src.name,MULTIOS_STREAM_CREATE,0,0,0));}
  else {TRY(found);if(dst.size>src.size)return BAD;off=dst.size;}
  while(off<src.size){
   TRY(progress(fn,ctx,off,src.size,MULTIOS_COPY_STREAM_DATA));
   u32 n=src.size-off>sizeof copy_buffer?sizeof copy_buffer:(u32)(src.size-off);
   TRY(fs_stream_read(source,src.name,off,copy_buffer,n));
   TRY(fs_stream_change(stage,src.name,MULTIOS_STREAM_APPEND,0,copy_buffer,n));off+=n;
  }
  TRY(compare_stream(source,stage,src.name,src.size,fn,ctx));
 }
 return r;
}
static int copy_impl(const char *source, const char *dest, const char *resume,
                     MultiCopyResult *result, MultiCopyProgress fn, void *ctx,int streams) {
    if(streams&&!(fs_capabilities()&MULTIOS_FS_NAMED_STREAMS))return UNSUP;
    if (!vol.mounted || !vol.writable || vol.poisoned) return READONLY;
    u32 len, slash, srclen, srcslash;
    TRY(path_length(source, &srclen, &srcslash));
    TRY(path_length(dest, &len, &slash));
    TRY(multi_name_valid(dest + slash));
    MultiFile64 identity;
    TRY(fs_open64(source, &identity));
    result->total = identity.size;
    /* FAT32 cannot represent 4 GiB. Fail before creating a staging file. */
    if (vol.kind == FS_FAT32 && identity.size > UINT32_MAX) return UNSUP;
    TRY(absent(dest));
    Stream parent; char leaf[PATHMAX];
    TRY(fs_parent(dest, &parent, leaf));
    uint64_t off = 0;
    if (resume) {
        u32 rn, rs;
        TRY(path_length(resume, &rn, &rs));
        if (rs != slash || !bytes_equal((const u8 *)dest, (const u8 *)resume, slash))
            return INVALID;
        MultiFile64 f; TRY(fs_open64(resume, &f));
        if ((f.data.first && f.data.first == identity.data.first) ||
            streq(source, resume) || streq(dest, resume)) return INVALID;
        if (!f.data.first && !identity.data.first && !f.size && !identity.size) {
            int alias = empty_alias(source, resume);
            if (alias < 0) return alias;
            if (alias) return INVALID;
        }
        if (f.size > identity.size) return BAD;
        mcopy(result->staging, resume, rn + 1);
        TRY(compare_prefix(source, &identity, resume, f.size, MULTIOS_COPY_PREFIX, fn, ctx));
        off = f.size;
    } else {
        if (slash + 12 >= PATHMAX) return TOOLONG;
        int found = 0;
        const char hex[] = "0123456789ABCDEF";
        for (u32 attempt = 0; attempt < 1024; attempt++) {
            char name[PATHMAX]; mcopy(name, dest, slash);
            u32 id = ++next_name; name[slash] = 'M'; name[slash + 1] = 'C';
            for (u32 i = 0; i < 6; i++) name[slash + 2 + i] = hex[(id >> (20 - 4 * i)) & 15];
            mcopy(name + slash + 8, ".TMP", 5);
            int equal = fs_names_equal(name + slash, dest + slash);
            if (equal < 0) return equal;
            if (equal) continue; /* staging must NEVER alias the final name */
            int r = absent(name);
            if (r == EXISTS) continue;
            if (r < 0) return r;
            TRY(progress(fn, ctx, 0, identity.size, MULTIOS_COPY_DATA));
            mcopy(result->staging, name, slash + 13);
            TRY(fs_put(name, 0, 0, 0, 0));
            found = 1; break;
        }
        if (!found) return EXISTS;
    }
    result->completed = off;
    while (off < identity.size) {
        TRY(progress(fn, ctx, off, identity.size, MULTIOS_COPY_DATA));
        MultiFile64 f; TRY(source_view(source, &identity, &f));
        u32 n = identity.size - off > sizeof copy_buffer ? sizeof copy_buffer : (u32)(identity.size - off);
        TRY(fs_read64(&f, off, copy_buffer, n));
        /* Verify the current staging length; never append to an unexpected view. */
        TRY(fs_open64(result->staging, &f));
        if (f.size != off) return BAD;
        TRY(fs_put(result->staging, copy_buffer, n, 2, 0));
        off += n; result->completed = off;
    }
    TRY(compare_prefix(source, &identity, result->staging, identity.size,
                       MULTIOS_COPY_VERIFY, fn, ctx));
    if(streams)TRY(copy_named_streams(source,result->staging,fn,ctx));
    TRY(absent(dest));
    TRY(fs_rename(result->staging, dest + slash));
    result->published = 1; return 0;
}
#endif
int fs_copy_staged(const char *source, const char *dest, const char *resume,
                   MultiCopyResult *result, MultiCopyProgress fn, void *ctx) {
    if (!result) return INVALID;
#ifdef MULTIOS_STORAGE_TX
    if (busy) return INVALID;
    mzero(result, sizeof *result); busy = 1;
    int r = copy_impl(source, dest, resume, result, fn, ctx,0);
    busy = 0; return r;
#else
    (void)source; (void)dest; (void)resume; (void)fn; (void)ctx;
    mzero(result, sizeof *result); return UNSUP;
#endif
}
int fs_copy_streams(const char*source,const char*dest,const char*resume,MultiCopyResult*result,MultiCopyProgress fn,void*ctx){
 if(!result)return INVALID;
#ifdef MULTIOS_STORAGE_TX
 if(busy)return INVALID;mzero(result,sizeof *result);busy=1;
 int r=copy_impl(source,dest,resume,result,fn,ctx,1);busy=0;return r;
#else
 (void)source;(void)dest;(void)resume;(void)fn;(void)ctx;mzero(result,sizeof *result);return UNSUP;
#endif
}
