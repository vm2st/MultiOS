/* vm2 studios | t.me/v2vproj — allocation ownership for embedded library. */
#define malloc mpntfs_malloc
#define calloc mpntfs_calloc
#define realloc mpntfs_realloc
#define free mpntfs_free
#define ntfs_free mpntfs_libfree
#define strdup mpntfs_strdup
#define time mpntfs_time

/* Embedded i386 runtime declarations: never infer an int return for pointers.
 * Some upstream translation units omit the corresponding libc headers. */
#ifdef MPNTFS_BIOS
extern int ffs(int);
extern char *mpntfs_strdup(const char *);
extern char *strerror(int);
#endif
