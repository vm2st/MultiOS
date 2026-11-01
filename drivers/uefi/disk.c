/* vm2 studios | t.me/v2vproj — boot-disk firmware adapter, not native USB. */
#include "platform/uefi/platform.h"
typedef struct {
    EFI_BLOCK_IO *io;
    UINT32 media_id;
    UINTN align;
    u32 sectors;
    int writable;
} BootDisk;
static BootDisk disk;
static u8 scratch[512+4096];
static void *buffer(void) {
    UINTN p=(UINTN)scratch, a=disk.align;
    return (void *)((p+a-1)&~(a-1));
}
static int present(void) {
    EFI_BLOCK_IO_MEDIA *m=disk.io->Media;
    return m && m->MediaPresent && m->MediaId==disk.media_id &&
        m->BlockSize==512 && m->LastBlock==(EFI_LBA)disk.sectors-1;
}
static int read_disk(void *ctx,u32 lba,u8 *out) {
    (void)ctx;
    if (!present() || lba>=disk.sectors) return IO;
    void *b=buffer();
    EFI_STATUS s=uefi_call_wrapper(disk.io->ReadBlocks,5,disk.io,disk.media_id,(EFI_LBA)lba,(UINTN)512,b);
    if (EFI_ERROR(s)) return IO;
    mcopy(out,b,512); return 0;
}
static int write_disk(void *ctx,u32 lba,const u8 *in) {
    (void)ctx;
    if (!present() || lba>=disk.sectors) return IO;
    if (!disk.writable || disk.io->Media->ReadOnly) return READONLY;
    void *b=buffer(); mcopy(b,in,512);
    return EFI_ERROR(uefi_call_wrapper(disk.io->WriteBlocks,5,disk.io,disk.media_id,(EFI_LBA)lba,(UINTN)512,b)) ? IO : 0;
}
static int flush_disk(void *ctx) {
    (void)ctx;
    if (!present()) return IO;
    if (!disk.writable) return 0;
    return EFI_ERROR(uefi_call_wrapper(disk.io->FlushBlocks,1,disk.io)) ? IO : 0;
}
/* Match the whole disk by EXACT parent path of the partition we booted from.
 * Never pick the first enumerated/internal disk as a fallback. */
static int open_disk(void *ctx,const char *options,MultiDevice *d,int ro) {
    if (*options) return INVALID;
    MultiEfi *p=ctx;
    EFI_DEVICE_PATH *boot=DevicePathFromHandle(p->boot_device), *node=boot;
    if (!boot) return UNSUP;
    UINTN prefix=0;
    while (!IsDevicePathEnd(node)) {
        UINTN n=DevicePathNodeLength(node);
        if (n<4 || prefix+n>4096) return UNSUP;
        if (DevicePathType(node)==MEDIA_DEVICE_PATH && node->SubType==MEDIA_HARDDRIVE_DP) break;
        prefix+=n; node=NextDevicePathNode(node);
    }
    if (IsDevicePathEnd(node) || !prefix) return UNSUP;
    EFI_HANDLE *handles=0; UINTN count=0;
    EFI_STATUS s=uefi_call_wrapper(p->st->BootServices->LocateHandleBuffer,5,ByProtocol,&BlockIoProtocol,0,&count,&handles);
    if (EFI_ERROR(s)) return IO;
    EFI_BLOCK_IO *found=0; int matches=0;
    for (UINTN i=0;i<count;i++) {
        EFI_DEVICE_PATH *path=DevicePathFromHandle(handles[i]);
        if (!path || DevicePathSize(path)!=prefix+4) continue;
        EFI_DEVICE_PATH *end=(EFI_DEVICE_PATH *)((u8 *)path+prefix);
        if (!IsDevicePathEnd(end) || CompareMem(path,boot,prefix)) continue;
        EFI_BLOCK_IO *io=0;
        s=uefi_call_wrapper(p->st->BootServices->HandleProtocol,3,handles[i],&BlockIoProtocol,(void **)&io);
        if (EFI_ERROR(s) || !io || !io->Media || io->Media->LogicalPartition) continue;
        found=io; matches++;
    }
    uefi_call_wrapper(p->st->BootServices->FreePool,1,handles);
    if (matches!=1) return NOTFOUND;
    EFI_BLOCK_IO_MEDIA *m=found->Media;
    if (!m->MediaPresent || m->BlockSize!=512 || m->LastBlock>=0x10000000u || !found->ReadBlocks) return UNSUP;
    UINTN align=m->IoAlign>1 ? m->IoAlign : 1;
    if (align>4096 || (align&(align-1))) return UNSUP;
    disk=(BootDisk){.io=found,.media_id=m->MediaId,.align=align,
        .sectors=(u32)m->LastBlock+1,.writable=!ro && !m->ReadOnly};
    if (disk.writable && (!found->WriteBlocks || !found->FlushBlocks)) return UNSUP;
    d->block=(MultiBlock){.sectors=disk.sectors,.sector_bytes=512,.writable=disk.writable,
        .read=read_disk,.write=write_disk,.flush=flush_disk};
    return 0;
}
static int open_rw(void *ctx,const char *options,MultiDevice *d) {return open_disk(ctx,options,d,0);}
static int open_ro(void *ctx,const char *options,MultiDevice *d) {return open_disk(ctx,options,d,1);}
const MultiDriver multi_uefi_disk=MULTI_DRIVER("uefi-boot-disk",MULTI_BLOCK,
    "vm2 studios: firmware boot disk, shared exFAT/FAT32/NTFS data",open_rw,0);
const MultiDriver multi_uefi_disk_ro=MULTI_DRIVER("uefi-boot-disk-ro",MULTI_BLOCK,
    "vm2 studios: firmware boot disk, forced RO",open_ro,0);
