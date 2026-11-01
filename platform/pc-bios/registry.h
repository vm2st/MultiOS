/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
static const MultiDriver *const multi_pc_registry[]={&multi_pc_text,&multi_pc_keys,&multi_pc_ata,&multi_pc_ata_ro,&multi_pc_usb_disk,&multi_pc_usb_disk_ro,&multi_pc_power};
static const MultiProfile multi_pc_default_profile={.id={"pc-vga-com1","pc-input-auto","pc-ata-pio","pc-8042-power"},.osk=0};
