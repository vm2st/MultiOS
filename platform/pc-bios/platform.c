/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform.h"
#include "platform/x86/cpu.h"
#include "multios/osk.h"
#include "multios/hardware.h"
#include "multios/boot_profile.h"
#ifndef MULTIOS_PC_REGISTRY
#define MULTIOS_PC_REGISTRY "platform/pc-bios/registry.h"
#endif
#include MULTIOS_PC_REGISTRY
void multipulti_main(void) {
    multi_x86_cpu_probe();
    MultiProfile profile=multi_pc_default_profile;
#ifdef MPNTFS_BIOS
    if(multi_boot_profile(&profile,(const unsigned char *)0x9e00)<0){
        volatile u16 *vga=(u16 *)0xb8000;
        const char *s="MultiPulti / vm2 studios: invalid BIOS configuration; restore image";
        for(int i=0;s[i];i++)vga[i]=0x4f00|(u8)s[i];
        for(;;)__asm__ volatile("cli;hlt");
    }
#endif
    int result=multi_modules_start(multi_pc_registry,sizeof(multi_pc_registry)/sizeof(multi_pc_registry[0]),
        &profile,0,"MultiPulti32 / vm2 studios / legacy BIOS / selected block provider",
        "x86 i386 protected mode",
        "Native VGA/PS2/COM1/ATA + UHCI/OHCI/EHCI keyboard and USB BOT disk. MODULES lists selected providers.\n"
        "No xHCI, external hubs, UAS, AHCI/NVMe/network.\n");
    if (result<0) {
        /* Emergency diagnostic is independent of the failed module stack. */
        volatile u16 *vga=(u16 *)0xb8000;
        const char *s="MultiPulti / vm2 studios: module boot failed; check PC profile";
        for (int i=0;s[i];i++) vga[i]=0x4f00|(u8)s[i];
        const char *reason=multi_modules_error();
        for (int i=0;reason[i] && i<79;i++) vga[80+i]=0x4f00|(u8)reason[i];
        for (;;) __asm__ volatile("cli;hlt");
    }
#ifdef MPNTFS_BIOS
    out("Extended BIOS NTFS profile: E820 RAM 1..30MiB verified by loader; bounded 24MiB arena.\n");
#endif
    out("Fault guard: i386 IDT + emergency double-fault stack active (fail-stop).\n");
    if(multi_hardware_usb(0)->io)multi_hardware_report(1);
    if (profile.osk==1) multi_osk_enable(1);
    shell();
}
