/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTI_H
#define MULTI_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
_Static_assert(UINT8_MAX == 255, "MultiOS requires 8-bit bytes");
_Static_assert(sizeof(u32) == 4, "MultiOS requires a 32-bit uint32_t");
#define PATHMAX 256
#define EOC 0xffffffffu
#define ATTR_DIR 16
#define TRY(x)                                                                                     \
    do {                                                                                           \
        int _r = (x);                                                                              \
        if (_r < 0)                                                                                \
            return _r;                                                                             \
    } while (0)
enum {
    IO = -1,
    BAD = -2,
    UNSUP = -3,
    NOTFOUND = -4,
    EXISTS = -5,
    NOSPACE = -6,
    READONLY = -7,
    NOTDIR = -8,
    ISDIR = -9,
    NOTEMPTY = -10,
    INVALID = -11,
    TOOLONG = -12,
    CANCELLED = -13
};
typedef struct {
    u32 first, size, valid;
    u8 flags;
    int root;
} Stream;
/* Extended read-only file view. Use path-based fs_write64/fs_resize64 for mutations.
 * Obtained from fs_open64; invalid after any mount attempt or allowed metadata mutation. Do not forge/edit. */
typedef struct {
    Stream data;
    uint64_t size, valid;
    u32 epoch;
    int kind;
} MultiFile64;
#define MULTI_STREAM_WIDE 0x80u /* legacy fs_read must refuse; use fs_read64 */
#define MULTI_ENTRY_SIZE64 1
#define MULTI_ENTRY_FEATURE 2
typedef struct {
    char name[256];
    char alias[13];
    Stream data;
    u16 attr;
    u32 pos;
    u8 count;
    int ascii, large;
    int name_valid; /* complete, valid UTF-8 name; not a truncated display label */
    uint64_t size64, valid64;
    int wide;
} Entry;
typedef struct {
    u32 base, sectors, fat, heap, clusters, root, spc, cluster_bytes;
    Stream bitmap, upcase;
    u16 flags;
    int mounted, writable, poisoned;
    u32 total_sectors;
    int kind;
} Volume;
extern Volume vol;
int disk_read(u32 lba, u8 *out);
int disk_write(u32 lba, const u8 *in);
int disk_flush(void);
u32 disk_sectors(void);
int disk_writable(void);
void out(const char *s);
void putch(char c);
void clear_screen(void);
int input_char(void);
void platform_exit(void);
void *mzero(void *p, u32 n);
void mcopy(void *d, const void *s, u32 n);
u32 slen(const char *s);
int streq(const char *a, const char *b);
int same(const char *a, const char *b);
u8 upper(u8 c);
void number(u32 n);
void hexnum(u32 n, int digits);
const char *errstr(int e);
enum { FS_EXFAT, FS_FAT32, FS_NTFS };
const char *fs_name(void);
/* 0 auto, 1..4 MBR partition, 5 raw volume */
int fs_mount_select(int selection);
int fs_mount(void);
Stream fs_root(void);
int fs_next(Stream dir, u32 *cursor, Entry *outentry);
int fs_lookup(Stream dir, const char *name, Entry *e);
int fs_resolve(const char *path, Stream *parent, Entry *entry, int *isroot);
int fs_parent(const char *path, Stream *parent, char *name);
int fs_read(Stream s, u32 off, u8 *buf, u32 n);
/* Absolute path; generic VFS read view, never a write permission. */
int fs_open64(const char *path, MultiFile64 *file);
int fs_read64(const MultiFile64 *file, uint64_t offset, u8 *buf, u32 count);
void number64(uint64_t value);
void hexnum64(uint64_t value);
int fs_free(u32 *count);
int fs_names_equal(const char *, const char *);
int fs_put(const char *path, const u8 *text, u32 n, int mode, const char *source);
/* Existing regular file only. write64 must remain within current size.
 * resize64 grows with zeros; one bounded transaction, not staged copy.
 * Successful/failed dispatched mutations invalidate every old view. */
int fs_write64(const char *path, uint64_t offset, const u8 *data, u32 count);
int fs_resize64(const char *path, uint64_t size);
/* Optional driver capabilities; names separate from file paths (no path:stream parsing).
 * Named streams refer to existing regular files. Restart enumeration after mutation. */
typedef struct {char name[768];uint64_t size,valid;u32 flags;} MultiNamedStream;
enum { MULTIOS_STREAM_CREATE, MULTIOS_STREAM_REPLACE, MULTIOS_STREAM_APPEND,
       MULTIOS_STREAM_WRITE, MULTIOS_STREAM_RESIZE, MULTIOS_STREAM_REMOVE };
int fs_stream_next(const char *,u32 *,MultiNamedStream *); /* 1 entry, 0 EOF */
int fs_stream_read(const char *,const char *,uint64_t,u8 *,u32);
int fs_stream_change(const char *,const char *,int,uint64_t,const u8 *,u32);
int fs_link(const char *source,const char *newpath);
int fs_remove(const char *path, int dir);
int fs_rename(const char *path, const char *newname);
int fs_unlock(void);
void shell(void);
#endif
