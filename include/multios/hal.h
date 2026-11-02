/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTIOS_HAL_H
#define MULTIOS_HAL_H
#include "multi.h"
#define MULTIOS_HAL_ABI 2u
/* A synchronous, single-threaded interface. No allocation, libc or CPU opcodes.
 * ctx belongs to the platform and must outlive shell()/all filesystem calls.
 * Read/write callbacks transfer exactly ONE complete 512-byte logical sector.
 * Return 0 on success, negative MultiOS error on failure (never short success).
 * The filesystem currently uses 32-bit LBA and supports one bound device.
 */
typedef struct {
    void *ctx;
    u32 sectors;
    u16 sector_bytes;
    int writable;
    int (*read)(void *ctx, u32 lba, u8 *sector);
    int (*write)(void *ctx, u32 lba, const u8 *sector);
    int (*flush)(void *ctx);
} MultiBlock;
typedef struct {
    void *ctx;
    void (*putc)(void *ctx, char ch);
    int (*getc)(void *ctx);   /* Blocking ASCII/key event (osk.h); -1 = EOF. */
    void (*clear)(void *ctx); /* Optional: clear screen. */
    void (*osk)(void *ctx, int enabled, int row, int col, int shift); /* Optional text panel. */
} MultiConsole;
typedef struct {
    u32 abi;
    const char *name;
    const char *architecture;
    MultiBlock block;
    MultiConsole console;
    void *lifecycle_ctx;
    void (*stop)(void *ctx);    /* May return in a hosted test harness. */
    int (*poweroff)(void *ctx); /* Optional, negative error if not available. */
    int (*reboot)(void *ctx);   /* Optional. */
    const char *drivers;        /* Only actual native/firmware capabilities, not wishes. */
} MultiHal;
/* Bind once before mounting. Rebinding a mounted filesystem is forbidden. */
int multi_hal_bind(const MultiHal *hal);
const MultiHal *multi_hal(void);
/* Sticky removal/failure notification from the currently bound block context.
 * No journal reset/rebind. ALL block callbacks stay disabled until a fresh
 * boot/bind, including direct storage/recovery access in transactional builds.
 * A notification raised inside an I/O callback overrides its zero result.
 * block_ctx must be the selected provider's non-NULL context, not another disk. */
int multi_hal_media_failed(void *block_ctx);
/* Read-only sticky status: true before bind or after selected-media failure.
 * Resetting storage bookkeeping does not clear this HAL-level latch. */
int multi_hal_media_unavailable(void);
#endif
