/* vm2 studios | t.me/v2vproj — static freestanding filesystem module ABI. */
#ifndef MULTIOS_FS_DRIVER_H
#define MULTIOS_FS_DRIVER_H
#include "multi.h"
#define MULTIOS_FS_ABI 5u
#define MULTIOS_FS_READ 1u
#define MULTIOS_FS_WRITE 2u
#define MULTIOS_FS_EXPERIMENTAL 4u
#define MULTIOS_FS_RANDOM_IO 8u
#define MULTIOS_FS_NAMED_STREAMS 16u
#define MULTIOS_FS_HARDLINKS 32u
typedef struct MultiFsDriver {
    u32 abi, size;
    int kind;
    const char *id, *name;
    u32 capabilities;
    int (*probe)(const u8 sector[512]);
    int (*partition_priority)(u8 type);
    int (*mount)(u32 base, u32 sectors);
    Stream (*root)(void);
    int (*next)(Stream, u32 *, Entry *);
    int (*read)(Stream, u32, u8 *, u32);
    int (*read64)(const MultiFile64 *, uint64_t, u8 *, u32);
    /* 1 equal, 0 unequal, negative error; follows this volume's case table. */
    int (*name_equal)(const char *, const char *);
    int (*free)(u32 *);
    int (*unlock)(void);
    int (*put)(const char *, const u8 *, u32, int, const char *);
    int (*remove)(const char *, int);
    int (*rename)(const char *, const char *);
    /* Optional pair: RANDOM_IO requires WRITE and both callbacks. */
    int (*write_at)(const char *, uint64_t, const u8 *, u32);
    int (*resize)(const char *, uint64_t);
    int (*stream_next)(const char *,u32 *,MultiNamedStream *);
    int (*stream_read)(const char *,const char *,uint64_t,u8 *,u32);
    int (*stream_change)(const char *,const char *,int,uint64_t,const u8 *,u32);
    int (*link)(const char *,const char *);
} MultiFsDriver;
int multi_fs_validate(const MultiFsDriver *const *registry, u32 count);
void fs_list_drivers(void);
u32 fs_capabilities(void);
#endif
