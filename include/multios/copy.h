/* vm2 studios | t.me/v2vproj — MultiPulti staged, resumable data copy.
 * MIT. Single writer, synchronous, prepared transactional profile only.
 * This copies DATA, not Windows ACLs/ADS/hardlinks/sparse allocation/timestamps.
 */
#ifndef MULTIOS_COPY_H
#define MULTIOS_COPY_H
#include "multi.h"
#define MULTIOS_COPY_CHUNK 8192u
enum { MULTIOS_COPY_PREFIX, MULTIOS_COPY_DATA, MULTIOS_COPY_VERIFY, MULTIOS_COPY_STREAM_DATA, MULTIOS_COPY_STREAM_VERIFY };
typedef struct {
    char staging[PATHMAX];
    uint64_t completed, total;
    int published;
} MultiCopyResult;
/* Return nonzero to cancel BEFORE publication. Must NOT reenter the VFS,
 * switch media/mount, or mutate files. No concurrent writers are supported. */
typedef int (*MultiCopyProgress)(void *, uint64_t, uint64_t, int);
/* Absolute normalized UTF-8 paths. Destination must not exist. resume=NULL
 * creates an exclusive MCxxxxxx.TMP in destination's parent; otherwise resume
 * must be an existing ordinary file in that same parent. Its ENTIRE prefix is
 * compared with source before any append. Existing mismatches are not changed.
 *
 * Each append is its own undo transaction. Only after complete byte comparison
 * is staging renamed to the final leaf in a final transaction. After failure
 * or reboot recover/remount first: staging may contain a committed prefix, or
 * publication may have committed if the last flush result was uncertain.
 * Never delete staging automatically on errors. result identifies it for
 * explicit resume/removal; completed is diagnostic, not a durable checkpoint.
 * No promise of whole-copy rollback, cross-volume copy, or metadata cloning.
 */
int fs_copy_staged(const char *, const char *, const char *, MultiCopyResult *,
                   MultiCopyProgress, void *);
/* Explicit DATA + named $DATA copy. NOT ACL/security/EA/reparse metadata clone.
 * Requires NAMED_STREAMS. Verifies all existing staging ADS prefixes; refuses
 * unknown/oversized/mismatching staging ADS. Each chunk is a transaction; final
 * rename publishes only after all DATA and ADS match. Resume is explicit. */
int fs_copy_streams(const char *,const char *,const char *,MultiCopyResult *,MultiCopyProgress,void *);
#endif