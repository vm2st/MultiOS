/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
/* Example compiled profile: text/OSK module is not linked into this image. */
static const MultiDriver *const multi_uefi_registry[]={
    &multi_uefi_plain,&multi_uefi_keys,&multi_uefi_file_ro,&multi_uefi_reset
};
static const MultiProfile multi_uefi_default_profile={
    .id={"uefi-plain","uefi-keys","uefi-file-ro","uefi-reset"}, .osk=0
};
