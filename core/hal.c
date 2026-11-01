/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "multios/hal.h"
#include "multios/osk.h"
#ifdef MULTIOS_STORAGE_TX
#include "multios/storage_tx.h"
#endif
static MultiHal active;
static int bound;
int multi_hal_bind(const MultiHal *h) {
    if (vol.mounted)
        return INVALID;
    if (!h || h->abi != MULTIOS_HAL_ABI || !h->name || !h->architecture || !h->console.putc ||
        !h->console.getc || !h->block.read || h->block.sector_bytes != 512 || !h->block.sectors ||
        h->block.sectors > 0x10000000u)
        return INVALID;
    if (h->block.writable && (!h->block.write || !h->block.flush))
        return INVALID;
    active = *h;
    bound = 1;
#ifdef MULTIOS_STORAGE_TX
    multi_storage_reset();
#endif
    return 0;
}
const MultiHal *multi_hal(void) {
    return bound ? &active : 0;
}
int multi_hal_media_failed(void *ctx){
 if(!bound||!ctx||ctx!=active.block.ctx)return INVALID;
 active.block.writable=0;
 if(vol.mounted){vol.writable=0;vol.poisoned=1;}
 return 0;
}
static int io_result(int r,int write_side) {
    if(r>0)r=IO; /* provider ABI: exactly zero or a negative error */
    if(r<0 && vol.mounted && (vol.writable || write_side)) {
        vol.poisoned=1;vol.writable=0;
    }
    return r;
}
int disk_read(u32 lba, u8 *b) {
    if (!bound || !b || lba >= active.block.sectors)
        return IO;
#ifdef MULTIOS_STORAGE_TX
    return io_result(multi_storage_read(lba,b),0);
#else
    return io_result(active.block.read(active.block.ctx, lba, b),0);
#endif
}
int disk_write(u32 lba, const u8 *b) {
    if (!bound || !b || lba >= active.block.sectors)
        return IO;
    if (!active.block.writable || (vol.mounted && vol.poisoned))
        return READONLY;
#ifdef MULTIOS_STORAGE_TX
    return io_result(multi_storage_write(lba,b),1);
#else
    return io_result(active.block.write(active.block.ctx, lba, b),1);
#endif
}
int disk_flush(void) {
    if (!bound)
        return IO;
#ifdef MULTIOS_STORAGE_TX
    return io_result(multi_storage_flush(),1);
#else
    if (active.block.flush)
        return io_result(active.block.flush(active.block.ctx),1);
    return active.block.writable ? IO : 0;
#endif
}
u32 disk_sectors(void) {
    return bound ? active.block.sectors : 0;
}
int disk_writable(void) {
    return bound && active.block.writable;
}
void putch(char c) {
    if (bound)
        active.console.putc(active.console.ctx, c);
}
void out(const char *s) {
    if (s)
        while (*s)
            putch(*s++);
}
void clear_screen(void) {
    if (bound && active.console.clear)
        active.console.clear(active.console.ctx);
}
int input_char(void) {
    if (!bound)
        return -1;
    for (;;) {
        int key = active.console.getc(active.console.ctx);
        int result = multi_osk_process(key);
        if (result != -2)
            return result;
    }
}
void platform_exit(void) {
    if (bound && active.stop)
        active.stop(active.lifecycle_ctx);
}
