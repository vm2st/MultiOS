/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/uefi/platform.h"
static EFI_SYSTEM_TABLE *system_table;
static int shutdown(void *ctx) {
    (void)ctx;
    if (disk_flush() < 0)
        return IO;
    uefi_call_wrapper(system_table->RuntimeServices->ResetSystem, 4, EfiResetShutdown, EFI_SUCCESS,
                      0, NULL);
    return UNSUP;
}
static int reboot(void *ctx) {
    (void)ctx;
    if (disk_flush() < 0)
        return IO;
    uefi_call_wrapper(system_table->RuntimeServices->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0,
                      NULL);
    return UNSUP;
}
static void stop(void *ctx) {
    shutdown(ctx);
    for (;;)
        __asm__ volatile("cli;hlt");
}

static void halt_only(void *ctx) {
    (void)ctx;
    for (;;) __asm__ volatile("cli;hlt");
}
static int open_reset(void *ctx, const char *options, MultiDevice *d) {
    if (*options) return INVALID;
    system_table=((MultiEfi *)ctx)->st;
    if (!system_table->RuntimeServices) return UNSUP;
    d->power=(MultiPower){.stop=stop,.poweroff=shutdown,.reboot=reboot}; return 0;
}
static int open_none(void *ctx, const char *options, MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->power=(MultiPower){.stop=halt_only}; return 0;
}
const MultiDriver multi_uefi_reset=MULTI_DRIVER("uefi-reset",MULTI_POWER,
    "firmware ResetSystem shutdown/reboot",open_reset,0);
const MultiDriver multi_uefi_no_reset=MULTI_DRIVER("uefi-no-reset",MULTI_POWER,
    "halt only; poweroff/reboot unavailable",open_none,0);
