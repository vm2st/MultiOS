/* vm2 studios | t.me/v2vproj — no native USB initialization. */
static const MultiDriver *const multi_pc_registry[]={&multi_pc_text,&multi_pc_keys,&multi_pc_ata,&multi_pc_power};
static const MultiProfile multi_pc_default_profile={.id={"pc-vga-com1","pc-input-auto","pc-ata-pio","pc-8042-power"},.options={"","ps2-only","",""},.osk=0};
