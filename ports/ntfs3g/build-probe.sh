#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
flags='-std=gnu11 -O2 -fpic -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -ffunction-sections -fdata-sections -DHAVE_CONFIG_H -D_GNU_SOURCE -DNO_NTFS_DEVICE_DEFAULT_IO_OPS -Iports/ntfs3g -Ivendor/ntfs-3g/include/ntfs-3g'
for name in acls attrib attrlist bitmap bootsect cache collate compress debug device dir ea efs index inode ioctl lcnalloc logfile mft misc mst object_id realpath reparse runlist security unistr volume xattrs; do
 gcc $flags -c vendor/ntfs-3g/libntfs-3g/$name.c -o build/ntfs3g/$name.o
done
