/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
/* UEFI BSP glue only. Boot Services remain active; drivers live in drivers/. */
#include "platform.h"
#include "platform/x86/cpu.h"
#ifndef MULTIOS_UEFI_REGISTRY
#define MULTIOS_UEFI_REGISTRY "platform/uefi/registry.h"
#endif
#include MULTIOS_UEFI_REGISTRY
static int read_config(EFI_FILE_HANDLE root, MultiProfile *p) {
    EFI_FILE_HANDLE f=0;
    EFI_STATUS s=uefi_call_wrapper(root->Open,5,root,&f,L"\\MULTIOS\\BOOT.CFG",EFI_FILE_MODE_READ,0);
    if (s==EFI_NOT_FOUND) return 0; /* absent config uses compiled defaults */
    if (EFI_ERROR(s)) return IO;
    char text[MULTI_CONFIG_LIMIT+1]; UINTN size=sizeof(text);
    s=uefi_call_wrapper(f->Read,3,f,&size,text);
    uefi_call_wrapper(f->Close,1,f);
    if (EFI_ERROR(s) || size>MULTI_CONFIG_LIMIT) return INVALID;
    return multi_profile_parse(p,text,(u32)size);
}
EFI_STATUS efi_main(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
    InitializeLib(image,st);
    multi_x86_cpu_probe();
    EFI_LOADED_IMAGE *loaded = 0;
    EFI_FILE_IO_INTERFACE *fs = 0;
    EFI_FILE_HANDLE root = 0;
    EFI_STATUS status;
    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3, image, &LoadedImageProtocol,
                               (void **)&loaded);
    if (EFI_ERROR(status))
        return status;
    status = uefi_call_wrapper(st->BootServices->HandleProtocol, 3, loaded->DeviceHandle,
                               &FileSystemProtocol, (void **)&fs);
    if (EFI_ERROR(status))
        return status;
    status = uefi_call_wrapper(fs->OpenVolume, 2, fs, &root);
    if (EFI_ERROR(status))
        return status;
    MultiProfile profile=multi_uefi_default_profile;
    int result=read_config(root,&profile);
    if (result<0) {
        Print(L"vm2 studios | MultiOS: invalid MULTIOS\\BOOT.CFG; boot stopped.\r\n");
        uefi_call_wrapper(root->Close,1,root); return EFI_INVALID_PARAMETER;
    }
    EFI_FILE_HANDLE flag=0;
    int osk_on=!EFI_ERROR(uefi_call_wrapper(root->Open,5,root,&flag,L"\\MULTIOS\\OSK.ON",EFI_FILE_MODE_READ,0));
    if (flag) uefi_call_wrapper(flag->Close,1,flag);
    MultiEfi platform={.st=st,.root=root,.boot_device=loaded->DeviceHandle};
    result=multi_modules_start(multi_uefi_registry,
        sizeof(multi_uefi_registry)/sizeof(multi_uefi_registry[0]), &profile,&platform,
        "UEFI x64 modular live / firmware-backed storage",
        "x86-64 / UEFI application (Boot Services stay active)",
        "MODULES lists selected providers. Firmware supplies USB/HID/storage coverage.\n"
        "Firmware console/input/block modules; filesystem backends selected by build profile. No native xHCI/AHCI/NVMe/network/touch drivers.\n");
    uefi_call_wrapper(root->Close,1,root);
    if (result<0) {
        Print(L"vm2 studios | MultiOS: module start failed: %a (error %d).\r\n",multi_modules_error(),result);
        return EFI_UNSUPPORTED;
    }
    clear_screen();
    if ((profile.osk<0 ? osk_on : profile.osk) && multi_osk_enable(1)<0) {
        out("OSK unavailable on this output module.\n");
    }
    shell(); return EFI_SUCCESS;
}
