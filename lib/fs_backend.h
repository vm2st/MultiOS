/* vm2 studios | t.me/v2vproj — static filesystem backend ABI */
#ifndef MULTIOS_FS_BACKEND_H
#define MULTIOS_FS_BACKEND_H
#include "multi.h"
#include "multios/fs_driver.h"
int multi_ntfs3g_mount(u32,u32);
int multi_ntfs3g_mount_full(u32,u32);
int multi_ntfs3g_next(Stream,u32*,Entry*);
int multi_ntfs3g_read(Stream,u32,u8*,u32);
int multi_ntfs3g_read64(const MultiFile64*,uint64_t,u8*,u32);
int multi_ntfs3g_equal(const char*,const char*);
int multi_ntfs3g_free(u32*);
int multi_ntfs3g_unlock(void);
int multi_ntfs3g_put(const char*,const u8*,u32,int,const char*);
int multi_ntfs3g_remove(const char*,int);
int multi_ntfs3g_rename(const char*,const char*);
int multi_fatfs_free(u32*);
int multi_fatfs_next(Stream,u32*,Entry*);
int multi_fatfs_read(Stream,u32,u8*,u32);
int multi_fatfs_read64(const MultiFile64*,uint64_t,u8*,u32);
int multi_fatfs_put(const char*,const u8*,u32,int,const char*);
int multi_fatfs_remove(const char*,int);
int multi_fatfs_rename(const char*,const char*);
int multi_fatfs_write_at(const char*,uint64_t,const u8*,u32);
int multi_fatfs_resize(const char*,uint64_t);
int multi_ntfs3g_write_at(const char*,uint64_t,const u8*,u32);
int multi_ntfs3g_resize(const char*,uint64_t);
int multi_ntfs3g_stream_next(const char*,u32*,MultiNamedStream*);
int multi_ntfs3g_stream_read(const char*,const char*,uint64_t,u8*,u32);
int multi_ntfs3g_stream_change(const char*,const char*,int,uint64_t,const u8*,u32);
int multi_ntfs3g_link(const char*,const char*);
extern const MultiFsDriver multi_fs_exfat, multi_fs_fat32, multi_fs_ntfs;
#define DECL_FS(n)                                                                                 \
    int n##_mount_at(u32 base, u32 limit);                                                         \
    Stream n##_root(void);                                                                         \
    int n##_next(Stream, u32 *, Entry *);                                                          \
    int n##_read(Stream, u32, u8 *, u32);                                                          \
    int n##_free(u32 *);                                                                           \
    int n##_unlock(void);                                                                          \
    int n##_put(const char *, const u8 *, u32, int, const char *);                                 \
    int n##_remove(const char *, int);                                                             \
    int n##_rename(const char *, const char *)
DECL_FS(exfat);
DECL_FS(fat32);
DECL_FS(ntfs);
int fat32_read64(const MultiFile64 *,uint64_t,u8 *,u32);
/* Partition-relative selected FAT and number of copies to update.
 * Geometry is validated by fat32_mount_at; no on-disk changes here. */
int fat32_fat_layout(u32 *start,u32 *length,u32 *copies);
int exfat_read64(const MultiFile64 *,uint64_t,u8 *,u32);
int ntfs_read64(const MultiFile64 *,uint64_t,u8 *,u32);
#endif
