/* MultiPulti | vm2 studios | t.me/v2vproj — experimental standalone undo journal.
 * NOT NTFS $LogFile. Optional VFS integration lives in storage_tx.c.
 * Single writer, 512-byte sectors, caller-owned reserved extent and media ID.
 * Assumes successful flush durably orders all previous writes. Never auto-formats.
 */
#ifndef MULTIOS_UNDO_H
#define MULTIOS_UNDO_H
#include "multios/block64.h"
#define MULTIOS_UNDO_MAX_RECORDS 4096u
#define MULTIOS_UNDO_SECTOR_BYTES 512u
#define MULTIOS_UNDO_FORMAT 1u
/* Half-open extents on ONE provider. The journal must be outside all live FS,
 * partitions, boot sectors and firmware data, not just outside this data extent.
 * The library cannot discover other owners: the image/BSP must enforce that.
 * media_id must be nonzero and stable; it is a binding tag, NOT authentication.
 */
typedef struct {
 uint64_t journal_start,journal_sectors,data_start,data_sectors;
 u8 media_id[16];
} MultiUndoConfig;
typedef struct {
 MultiBlock64 *io;
 MultiUndoConfig config;
 uint64_t sequence,transaction,targets[MULTIOS_UNDO_MAX_RECORDS];
 u32 count,capacity,mirror_first,expected_crc[MULTIOS_UNDO_MAX_RECORDS];
 int ready,active,failed,owned,degraded;
} MultiUndo;
/* Initialize the complete reserved extent to zero OUTSIDE this API, only on a
 * NEW disposable image. format refuses any nonzero sector and is explicit.
 * Interrupted initial format may need open/recovery; never reformat to recover.
 */
int multi_undo_format(MultiBlock64 *,const MultiUndoConfig *);
/* Zero-initialize a fresh object. An ACTIVE result is ready but must be recovered
 * before the protected filesystem is mounted. open never writes any sector.
 */
int multi_undo_open(MultiUndo *,MultiBlock64 *,const MultiUndoConfig *);
/* begin reserves generation space for a full transaction. NOSPACE on write
 * requires abort/recover, never a successful partial filesystem operation.
 * Only a transaction begun by this live object may be written/committed;
 * reopened ACTIVE transactions must be recovered first. After any commit I/O error the result is UNKNOWN until fresh open/recovery. */
int multi_undo_begin(MultiUndo *);
int multi_undo_read(MultiUndo *,uint64_t,u8 *);
int multi_undo_write(MultiUndo *,uint64_t,const u8 *);
int multi_undo_commit(MultiUndo *);
/* Recover/abort restores pre-transaction sectors, validates all records first.
 * Restartable after interruption. Requires a healthy writable provider; after
 * I/O failure discard both objects, reopen the device and journal before retry.
 * Success is not permission to mount an unsupported/dirty filesystem writable.
 */
int multi_undo_recover(MultiUndo *);
#endif
