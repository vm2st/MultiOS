/* vm2 studios | t.me/v2vproj — bounded NTFS attribute readers, no disk writes. */
#ifndef MULTIOS_NTFS_ATTR_H
#define MULTIOS_NTFS_ATTR_H
#include "multi.h"
typedef struct {
    const u8 *raw, *name, *value, *mapping;
    u32 type, bytes, value_bytes, mapping_bytes;
    u16 id, flags, compression_unit;
    u8 name_units, nonresident;
    uint64_t first_vcn, last_vcn, allocated, size, initialized;
} MultiNtfsAttr;
typedef struct {
    const u8 *name;
    u32 type, bytes;
    u16 id, sequence;
    u8 name_units;
    uint64_t first_vcn, record;
} MultiNtfsAttrRef;
/* Record has already passed USA decoding. Cursor starts at FILE.attr_offset.
 * Returns 1=item, 0=end, negative=error. Cursor/result unchanged on error/end.
 */
int ntfs_attr_next(const u8 *record,u32 bytes,u32 *cursor,MultiNtfsAttr *attr);
/* Attribute-list value has an exact byte size; no implicit terminator/padding.
 * UTF-16 names remain views, not lossy ASCII strings. */
int ntfs_attrlist_next(const u8 *value,u32 bytes,u32 *cursor,MultiNtfsAttrRef *ref);
#endif
