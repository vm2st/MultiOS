#include "multios/hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;
static u8 sectors[2][512];
static int reads, writes, flushes, stops, clears;
static char output[64];
static int length;
static int read_sector(void *ctx, u32 lba, u8 *b) {
    assert(ctx == sectors);
    reads++;
    memcpy(b, sectors[lba], 512);
    return 0;
}
static int write_sector(void *ctx, u32 lba, const u8 *b) {
    assert(ctx == sectors);
    writes++;
    memcpy(sectors[lba], b, 512);
    return 0;
}
static int flush(void *ctx) {
    assert(ctx == sectors);
    flushes++;
    return 0;
}
static void emit(void *ctx, char c) {
    assert(ctx == output);
    output[length++] = c;
}
static int key(void *ctx) {
    assert(ctx == output);
    return 'x';
}
static void clear(void *ctx) {
    assert(ctx == output);
    clears++;
}
static void stop(void *ctx) {
    assert(ctx == &stops);
    stops++;
}
int main(void) {
    u8 buf[512];
    assert(multi_hal() == 0);
    assert(disk_read(0, buf) == IO);
    assert(input_char() == -1);
    MultiHal h = {.abi = MULTIOS_HAL_ABI,
                  .name = "memory test port",
                  .architecture = "native",
                  .block = {.ctx = sectors,
                            .sectors = 2,
                            .sector_bytes = 512,
                            .writable = 1,
                            .read = read_sector,
                            .write = write_sector,
                            .flush = flush},
                  .console = {.ctx = output, .putc = emit, .getc = key, .clear = clear},
                  .lifecycle_ctx = &stops,
                  .stop = stop};
    assert(multi_hal_bind(0) == INVALID);
    h.abi = MULTIOS_HAL_ABI + 1;
    assert(multi_hal_bind(&h) == INVALID);
    h.abi = MULTIOS_HAL_ABI;
    h.block.sector_bytes = 4096;
    assert(multi_hal_bind(&h) == INVALID);
    h.block.sector_bytes = 512;
    h.block.flush = 0;
    assert(multi_hal_bind(&h) == INVALID);
    h.block.flush = flush;
    assert(multi_hal_bind(&h) == 0);
    h.block.sectors = 99;
    assert(disk_sectors() == 2);
    h.block.sectors = 2;
    memset(buf, 0xa5, 512);
    assert(disk_write(1, buf) == 0);
    memset(buf, 0, 512);
    assert(disk_read(1, buf) == 0);
    assert(buf[0] == 0xa5 && buf[511] == 0xa5);
    assert(disk_read(2, buf) == IO);
    assert(disk_write(2, buf) == IO);
    assert(reads == 1 && writes == 1);
    assert(disk_read(0, 0) == IO);
    assert(disk_write(0, 0) == IO);
    assert(disk_flush() == 0 && flushes == 1);
    out("hello");
    assert(length == 5 && !memcmp(output, "hello", 5));
    assert(input_char() == 'x');
    clear_screen();
    assert(clears == 1);
    platform_exit();
    assert(stops == 1);
    assert(multi_hal_media_failed(0)==INVALID);
    assert(multi_hal_media_failed(output)==INVALID&&disk_writable());
    vol.mounted=vol.writable=1;
    assert(multi_hal_media_failed(sectors)==0&&!disk_writable()&&!vol.writable&&vol.poisoned);
    assert(disk_write(0,buf)==READONLY);
    assert(multi_hal_bind(&h) == INVALID);
    vol.mounted = 0;vol.poisoned=0;
    h.block.writable = 0;
    h.block.write = 0;
    h.block.flush = 0;
    h.console.clear = 0;
    h.stop = 0;
    assert(multi_hal_bind(&h) == 0);
    assert(disk_write(0, buf) == READONLY);
    assert(!disk_writable());
    assert(disk_flush() == 0);
    clear_screen();
    platform_exit();
    puts("HAL contract tests passed (binding, contexts, I/O bounds, RO, lifecycle, sticky selected-media failure)");
    return 0;
}
