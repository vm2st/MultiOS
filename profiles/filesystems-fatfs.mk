# vm2 studios | t.me/v2vproj — opt-in upstream FAT32/exFAT mutation engine.
include profiles/filesystems-journal.mk
FS_DRIVERS += lib/fatfs_core.c lib/fatfs_unicode.c lib/fatfs_adapter.c
FS_TX_CFLAGS += -DMULTIOS_FATFS -Ivendor/fatfs/port -ffunction-sections -fdata-sections

FS_KLDFLAGS = --gc-sections
FS_DRIVERS += lib/div64.c

EFILDS = platform/uefi/sections.ld
