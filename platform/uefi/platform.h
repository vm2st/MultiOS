/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTIOS_UEFI_PLATFORM_H
#define MULTIOS_UEFI_PLATFORM_H
#include "multios/modules.h"
#include "multios/osk.h"
#include <efi.h>
#include <efilib.h>
typedef struct { EFI_SYSTEM_TABLE *st; EFI_FILE_HANDLE root; EFI_HANDLE boot_device; } MultiEfi;
extern const MultiDriver multi_uefi_text, multi_uefi_plain, multi_uefi_keys, multi_uefi_file, multi_uefi_file_ro;
extern const MultiDriver multi_uefi_disk, multi_uefi_disk_ro;
extern const MultiDriver multi_uefi_reset, multi_uefi_no_reset;
#endif
