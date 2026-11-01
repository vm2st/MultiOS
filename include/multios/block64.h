/* vm2 studios | t.me/v2vproj — freestanding sector I/O foundation, ABI 1.
 * This does not replace legacy HAL ABI 2 and does not implement journaling.
 */
#ifndef MULTIOS_BLOCK64_H
#define MULTIOS_BLOCK64_H
#include "multi.h"
#define MULTIOS_BLOCK64_ABI 1u
typedef struct {
    u32 abi, sector_bytes;
    uint64_t sectors;
    void *ctx;
    int writable;
    int (*read)(void *ctx,uint64_t lba,u8 *sector);
    int (*write)(void *ctx,uint64_t lba,const u8 *sector);
    int (*flush)(void *ctx);
} MultiBlock64Provider;
typedef struct { MultiBlock64Provider provider; int bound, failed; } MultiBlock64;
/* Initialize the state to zero. No rebinding: create a new state for a new mount.
 * Providers transfer exactly one sector and return 0 or a negative error.
 */
int multi_block64_bind(MultiBlock64 *io,const MultiBlock64Provider *provider);
int multi_block64_read(MultiBlock64 *io,uint64_t lba,u8 *sector);
int multi_block64_write(MultiBlock64 *io,uint64_t lba,const u8 *sector);
int multi_block64_flush(MultiBlock64 *io);
/* Caller-supplied scratch >=sector_bytes, non-overlapping output/scratch/provider.
 * Zero length is a no-op; failed transfers may leave a partially filled output.
 * No unaligned write/RMW API: future metadata transactions must order full sectors.
 */
int multi_block64_read_bytes(MultiBlock64 *io,uint64_t offset,u8 *out,u32 bytes,
                             u8 *scratch,u32 scratch_bytes);
#endif
