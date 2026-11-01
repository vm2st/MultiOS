/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
/* Replace this registry through profiles/uefi.mk for another UEFI BSP. */
static const MultiDriver *const multi_uefi_registry[]={
    &multi_uefi_text, &multi_uefi_plain, &multi_uefi_keys, &multi_uefi_file, &multi_uefi_file_ro,
    &multi_uefi_disk, &multi_uefi_disk_ro, &multi_uefi_reset, &multi_uefi_no_reset
};
static const MultiProfile multi_uefi_default_profile={
    .id={"uefi-text","uefi-keys","uefi-file","uefi-reset"}, .osk=-1
};
