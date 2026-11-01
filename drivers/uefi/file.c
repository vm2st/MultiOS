/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/uefi/platform.h"
static EFI_FILE_HANDLE backing;
static int canwrite;
static int read_sector(void *ctx, u32 lba, u8 *b) {
    (void)ctx;
    UINTN size = 512;
    EFI_STATUS s = uefi_call_wrapper(backing->SetPosition, 2, backing, (UINT64)lba * 512);
    if (EFI_ERROR(s))
        return IO;
    s = uefi_call_wrapper(backing->Read, 3, backing, &size, b);
    return !EFI_ERROR(s) && size == 512 ? 0 : IO;
}
static int write_sector(void *ctx, u32 lba, const u8 *b) {
    (void)ctx;
    if (!canwrite)
        return READONLY;
    UINTN size = 512;
    EFI_STATUS s = uefi_call_wrapper(backing->SetPosition, 2, backing, (UINT64)lba * 512);
    if (EFI_ERROR(s))
        return IO;
    s = uefi_call_wrapper(backing->Write, 3, backing, &size, (void *)b);
    return !EFI_ERROR(s) && size == 512 ? 0 : IO;
}
static int flush(void *ctx) {
    (void)ctx;
    if (!canwrite)
        return 0;
    return EFI_ERROR(uefi_call_wrapper(backing->Flush, 1, backing)) ? IO : 0;
}

static void close_file(MultiDevice *d) {
    (void)d;
    if (backing) uefi_call_wrapper(backing->Close,1,backing);
    backing=0;
}
static int open_file(void *ctx, const char *options, MultiDevice *d, int ro) {
    MultiEfi *p=ctx;
    /* Empty option is the safe default; explicit paths are absolute ASCII. */
    const char *path=*options ? options : "\\MULTIOS\\DATA.IMG";
    if (path[0]!='\\') return INVALID;
    CHAR16 wide[MULTI_MODULE_OPTIONS]; u32 n=0;
    while (path[n]) {
        if (n>=MULTI_MODULE_OPTIONS-1 || (u8)path[n]<32 || (u8)path[n]>126 || path[n]=='/' || path[n]==':') return INVALID;
        wide[n]=(u8)path[n]; n++;
    }
    wide[n]=0; backing=0; canwrite=0;
    EFI_STATUS s=EFI_UNSUPPORTED;
    if (!ro) s=uefi_call_wrapper(p->root->Open,5,p->root,&backing,wide,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
    canwrite=!EFI_ERROR(s);
    if (!canwrite) s=uefi_call_wrapper(p->root->Open,5,p->root,&backing,wide,EFI_FILE_MODE_READ,0);
    if (EFI_ERROR(s)) return IO;
    UINT64 length=0;
    s=uefi_call_wrapper(backing->SetPosition,2,backing,~(UINT64)0);
    if (!EFI_ERROR(s)) s=uefi_call_wrapper(backing->GetPosition,2,backing,&length);
    if (EFI_ERROR(s) || length<512 || length%512 || length/512>0x10000000u) {
        close_file(d); return INVALID;
    }
    d->block=(MultiBlock){.sectors=(u32)(length/512),.sector_bytes=512,
        .writable=canwrite,.read=read_sector,.write=write_sector,.flush=flush};
    return 0;
}
static int file_rw(void *ctx, const char *options, MultiDevice *d) { return open_file(ctx,options,d,0); }
static int file_ro(void *ctx, const char *options, MultiDevice *d) { return open_file(ctx,options,d,1); }
const MultiDriver multi_uefi_file=MULTI_DRIVER("uefi-file",MULTI_BLOCK,
    "firmware file image; experimental RW capability",file_rw,close_file);
const MultiDriver multi_uefi_file_ro=MULTI_DRIVER("uefi-file-ro",MULTI_BLOCK,
    "firmware file image; forced read-only",file_ro,close_file);
