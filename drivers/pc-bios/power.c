/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/pc-bios/io.h"
extern void multi_usb_stop(void) __attribute__((weak));
static int pc_reboot(void *ctx) {
    (void)ctx;
    for (u32 n = 0; n < 100000; n++)
        if (!(ib(0x64) & 2)) {
            ob(0x64, 0xfe);
            break;
        }
    return UNSUP;
}
static void pc_stop(void *ctx) {
    (void)ctx;
    if(multi_usb_stop)multi_usb_stop();
    ob(0xf4, 0x10);
    for (;;)
        __asm__ volatile("cli; hlt");
}
static int open_power(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->power=(MultiPower){.stop=pc_stop,.reboot=pc_reboot}; return 0;
}
const MultiDriver multi_pc_power=MULTI_DRIVER("pc-8042-power",MULTI_POWER,
    "8042 reboot / halt; no native poweroff",open_power,0);
