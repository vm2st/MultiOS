# GPL combined-build experimental profile: x64 UEFI + host harness ONLY.
# BIOS retains the smaller legacy NTFS reader; do not use this profile for all.
include profiles/filesystems-fatfs.mk
FS_DRIVERS += lib/ntfs3g_adapter.c
FS_TX_CFLAGS += -DMULTIOS_NTFS3G -DHAVE_CONFIG_H -D_GNU_SOURCE -Iports/ntfs3g -Ivendor/ntfs-3g/include/ntfs-3g
NTFS_NAMES = acls attrib attrlist bitmap bootsect cache collate compress debug device dir ea efs index inode ioctl lcnalloc logfile mft misc mst object_id realpath reparse runlist security unistr xattrs volume_port log allocator
NTFS_HOST_OBJ = $(addprefix build/ntfs3g-host/,$(addsuffix .o,$(NTFS_NAMES)))
NTFS_EOBJ = $(addprefix build/ntfs3g-efi/,$(addsuffix .o,$(NTFS_NAMES))) build/ntfs3g-efi/runtime.o

NTFS_LDFLAGS = --gc-sections --no-undefined --version-script=ports/ntfs3g/exports.map

NTFS_HOST_SAN_OBJ = $(patsubst build/ntfs3g-host/%,build/ntfs3g-san/%,$(NTFS_HOST_OBJ))
