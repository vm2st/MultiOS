/* MultiPulti | vm2 studios | t.me/v2vproj — opt-in filesystem undo adapter. */
#ifndef MULTIOS_STORAGE_TX_H
#define MULTIOS_STORAGE_TX_H
#include "multi.h"
void multi_storage_reset(void);
/* Selected HAL medium was lost: no reads, writes, flush, commit or recovery
 * through this binding. Preserve the journal for a fresh bind/reopen. */
void multi_storage_media_failed(void);
/* Recover BEFORE filesystem mount; missing locator permits read-only only. */
int multi_storage_prepare(void);
int multi_storage_matches(u32 base,u32 sectors);
int multi_storage_begin(void);
/* Record a rejected HAL/preflight request; no disk I/O, first error wins.
 * Does not invalidate a healthy provider or prevent a logical rollback. */
int multi_storage_reject(int error);
int multi_storage_read(u32 lba,u8 *sector);
int multi_storage_flush(void);
int multi_storage_write(u32 lba,const u8 *sector);
/* Any failed operation invalidates the mount, even after successful rollback. */
int multi_storage_finish(int operation_result);
int multi_storage_blocked(void);
const char *multi_storage_description(void);
#endif
