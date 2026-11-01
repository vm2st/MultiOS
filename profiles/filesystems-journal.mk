# vm2 studios | t.me/v2vproj — opt-in experimental WHOLE-OPERATION undo profile.
# Prepared image required for RW. Missing footer -> RO, never unsafe fallback.
# NTFS is STILL read-only. This is NOT a stable Release 1.0 profile.
include profiles/filesystems.mk
FS_DRIVERS += lib/undo.c lib/storage_tx.c
FS_TX_CFLAGS = -DMULTIOS_STORAGE_TX
