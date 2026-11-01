/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/pc-bios/io.h"
static u32 sectors;
static int can_flush,can_write;
static void delay(void) {
    ib(0x3f6);
    ib(0x3f6);
    ib(0x3f6);
    ib(0x3f6);
}
static int waitata(int drq) {
    for (u32 i = 0; i < 10000000; i++) {
        u8 s = ib(0x1f7);
        if (s == 0 || s == 0xff)
            return IO;
        if (!(s & 0x80)) {
            if (s & 0x21)
                return IO;
            if (drq ? (s & 8) : !(s & 8))
                return 0;
        }
    }
    return IO;
}
static int selectlba(u32 lba) {
    if (lba >= sectors || lba > 0x0fffffffu)
        return IO;
    TRY(waitata(0));
    ob(0x1f6, 0xe0 | ((lba >> 24) & 15));
    delay();
    ob(0x1f2, 1);
    ob(0x1f3, lba);
    ob(0x1f4, lba >> 8);
    ob(0x1f5, lba >> 16);
    return 0;
}
static int pc_read(void *ctx, u32 lba, u8 *b) {
    (void)ctx;
    TRY(selectlba(lba));
    ob(0x1f7, 0x20);
    delay();
    TRY(waitata(1));
    for (int i = 0; i < 256; i++) {
        u16 w = iw(0x1f0);
        b[i * 2] = w;
        b[i * 2 + 1] = w >> 8;
    }
    delay();
    return waitata(0);
}
#ifdef MULTIOS_BIOS_FAULT_TEST
/* Test bundle ONLY. Production binary has neither hooks nor a fault command.
 * Loader reads per-test threshold/mode from header into reserved low RAM. */
static u32 test_writes;static int test_failed;
u32 multi_bios_test_writes(void){return test_writes;}
static int test_before_write(void){
 if(test_failed)return IO;
 const volatile u32*config=(const volatile u32*)0x7100;
 if(++test_writes&&config[0]&&test_writes==config[0]){
  if(config[1]==1){out("\nBIOS TEST PROCESS STOP before selected raw write\n");ob(0xf4,0x11);for(;;)__asm__ volatile("cli;hlt");}
  test_failed=1;out("\nBIOS TEST sticky raw I/O error\n");return IO;
 }return 0;
}
#endif
static int pc_write(void *ctx, u32 lba, const u8 *b) {
#ifdef MULTIOS_BIOS_FAULT_TEST
    TRY(test_before_write());
#endif
    (void)ctx;
    if(!can_write)return READONLY;
    TRY(selectlba(lba));
    ob(0x1f7, 0x30);
    delay();
    TRY(waitata(1));
    for (int i = 0; i < 256; i++)
        ow(0x1f0, b[i * 2] | ((u16)b[i * 2 + 1] << 8));
    delay();
    return waitata(0);
}
static int pc_flush(void *ctx) {
    (void)ctx;
    if (!sectors) return IO;
    if (!can_write) return 0; /* RO provider cannot have submitted writes. */
    if (!can_flush) return 0; /* Valid IDENTIFY proves write cache disabled/absent. */
    TRY(waitata(0));
    ob(0x1f7, 0xe7);
    delay();
    return waitata(0);
}
static int identify(void) {
    sectors=0;can_flush=can_write=0;
    ob(0x3f6, 2);
    ob(0x1f6, 0xa0);
    delay();
    ob(0x1f2, 0);
    ob(0x1f3, 0);
    ob(0x1f4, 0);
    ob(0x1f5, 0);
    ob(0x1f7, 0xec);
    delay();
    TRY(waitata(1));
    u16 id[256];
    for (int i = 0; i < 256; i++)
        id[i] = iw(0x1f0);
    TRY(waitata(0));
    /* No ATAPI or >512-byte logical sectors may enter the 512-byte VFS. */
    if ((id[0]&0x8000) || ((id[106]&0xc000)==0x4000 && (id[106]&0x1000) && (id[117]!=256 || id[118]))) return UNSUP;
    if (!(id[49] & 0x200))
        return UNSUP;
    /* ATA command-set validity must be explicit. Unknown cache semantics are
     * read-only, never a fictitious successful durability barrier. */
    int commands_valid=(id[83]&0xc000)==0x4000;
    int enabled_valid=(id[87]&0xc000)==0x4000;
    can_flush=commands_valid && (id[83]&0x1000);
    can_write=can_flush || (commands_valid && (!(id[82]&0x20) || (enabled_valid && !(id[85]&0x20))));
    sectors = id[60] | ((u32)id[61] << 16);
    if (sectors > 0x10000000)
        sectors = 0x10000000;
    return sectors ? 0 : IO;
}
static int open_ata(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    TRY(identify());
    /* Do not silently use an unrelated internal exFAT disk after USB BIOS boot.
     * This is an identity guard, not proof of controller mapping or security. */
    u8 mbr[512]; TRY(pc_read(0,0,mbr));
    const volatile u8 *boot_id=(const volatile u8 *)(0x7c00+440);
    int nonzero=0;
    for (int i=0;i<4;i++) { nonzero|=boot_id[i]; if (mbr[440+i]!=boot_id[i]) return NOTFOUND; }
    if (!nonzero) return UNSUP;
    d->block=(MultiBlock){.sectors=sectors,.sector_bytes=512,.writable=can_write,
        .read=pc_read,.write=pc_write,.flush=pc_flush}; return 0;
}
const MultiDriver multi_pc_ata=MULTI_DRIVER("pc-ata-pio",MULTI_BLOCK,
    "native ATA PIO primary master, boot MBR-ID guard",open_ata,0);

static int open_ata_ro(void *ctx,const char *options,MultiDevice *d){
 int rc=open_ata(ctx,options,d);if(rc<0)return rc;
 d->block.writable=0;d->block.write=0;return 0;
}
const MultiDriver multi_pc_ata_ro=MULTI_DRIVER("pc-ata-pio-ro",MULTI_BLOCK,
 "native ATA primary master, enforced read-only provider",open_ata_ro,0);
