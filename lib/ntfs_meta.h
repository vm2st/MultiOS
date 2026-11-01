/* vm2 studios | t.me/v2vproj — freestanding NTFS metadata codecs.
 * Codecs NEVER perform disk I/O or replay $LogFile. Encoding is not a safe write policy.
 */
#ifndef MULTIOS_NTFS_META_H
#define MULTIOS_NTFS_META_H
#include "multi.h"
typedef struct {
    uint64_t vcn, lcn, length;
    int sparse;
} MultiNtfsRun;
/* Failure leaves the buffer unchanged. Accepts 512..4096-byte power-of-two sectors.
 * Caller validates FILE/INDX signature and record-specific header/attribute bounds.
 */
int ntfs_usa_decode(u8 *record, u32 bytes, u32 sector_bytes);
int ntfs_usa_encode(u8 *record, u32 bytes, u32 sector_bytes);
/* Mapping-pairs codecs: 64-bit VCN/LCN/length, signed relative LCN, sparse runs.
 * Input/output ranges must not overlap.
 * No output/count/next changes on failure; terminator required, padding after it ignored.
 * cluster_count is physical volume capacity, not a limit on a sparse stream's logical size.
 */
int ntfs_runs_decode(const u8 *data, u32 bytes, uint64_t first_vcn,
                     uint64_t cluster_count, MultiNtfsRun *out, u32 capacity,
                     u32 *count, uint64_t *next_vcn);
int ntfs_runs_encode(const MultiNtfsRun *runs, u32 count, uint64_t cluster_count,
                     u8 *data, u32 capacity, u32 *written);
#endif
