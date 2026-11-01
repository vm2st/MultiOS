/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#define _POSIX_C_SOURCE 200809L
#include "multios/modules.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int fd;
static u32 sectors;
static int canwrite;
static int host_read(void *ctx, u32 lba, u8 *b) {
    (void)ctx;
    if (lba >= sectors)
        return IO;
    return pread(fd, b, 512, (off_t)lba * 512) == 512 ? 0 : IO;
}
static int host_write(void *ctx, u32 lba, const u8 *b) {
    (void)ctx;
#ifdef MULTIOS_FAULT_INJECTION
    static long count = 0;
    const char *cut = getenv("MULTIOS_FAIL_WRITE_AT");
    if (cut && count++ == strtol(cut, 0, 10))
        return IO;
#endif
    if (!canwrite || lba >= sectors)
        return IO;
    return pwrite(fd, b, 512, (off_t)lba * 512) == 512 ? 0 : IO;
}
static int host_flush(void *ctx) {
    (void)ctx;
#ifdef MULTIOS_FAULT_INJECTION
    static long count=0;
    const char *cut=getenv("MULTIOS_FAIL_FLUSH_AT");
    if(cut && count++==strtol(cut,0,10))return IO;
#endif
    return canwrite && fsync(fd) < 0 ? IO : 0;
}
static void host_putc(void *ctx, char c) {
    (void)ctx;
    putchar(c);
    fflush(stdout);
}
static void host_clear(void *ctx) {
    (void)ctx;
    fputs("\033[2J\033[H", stdout);
    fflush(stdout);
}
static int host_getc(void *ctx) {
    (void)ctx;
    return getchar();
}
static void host_stop(void *ctx) {
    (void)ctx;
    close(fd);
}
static int open_output(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->output=(MultiOutput){.putc=host_putc,.clear=host_clear}; return 0;
}
static int open_input(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->input=(MultiInput){.getc=host_getc}; return 0;
}
static int open_block(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->block=(MultiBlock){.sectors=sectors,.sector_bytes=512,.writable=canwrite,
        .read=host_read,.write=host_write,.flush=host_flush}; return 0;
}
static int open_power(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    d->power=(MultiPower){.stop=host_stop}; return 0;
}
static const MultiDriver host_modules[]={
    MULTI_DRIVER("posix-output",MULTI_OUTPUT,"test stdout",open_output,0),
    MULTI_DRIVER("posix-input",MULTI_INPUT,"test stdin",open_input,0),
    MULTI_DRIVER("posix-image",MULTI_BLOCK,"test regular-file image",open_block,0),
    MULTI_DRIVER("posix-stop",MULTI_POWER,"test close file",open_power,0)
};
int main(int argc, char **argv) {
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--allow-write"))) {
        fprintf(stderr, "Usage: %s regular-image-file [--allow-write]\n", argv[0]);
        return 2;
    }
    canwrite = argc == 3;
    fd = open(argv[1], (canwrite ? O_RDWR : O_RDONLY) | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        perror("open");
        return 2;
    }
    struct stat s;
    if (fstat(fd, &s) || !S_ISREG(s.st_mode) || s.st_nlink != 1 || s.st_size < 512 || s.st_size % 512 ||
        s.st_size / 512 > 0x10000000) {
        fprintf(stderr,
                "Only regular sector-aligned image files <=128 GiB accepted. No block devices.\n");
        return 2;
    }
    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        perror("image locked");
        return 2;
    }
    struct flock lock = {.l_type=canwrite ? F_WRLCK : F_RDLCK,.l_whence=SEEK_SET};
    if (fcntl(fd,F_SETLK,&lock)<0) { perror("image record-locked");close(fd);return 2; }
    sectors = s.st_size / 512;
    const MultiDriver *registry[]={&host_modules[0],&host_modules[1],&host_modules[2],&host_modules[3]};
    MultiProfile profile={.id={"posix-output","posix-input","posix-image","posix-stop"},.osk=0};
    if (multi_modules_start(registry,4,&profile,0,"POSIX image harness (not a booted OS)",
        "native compiler target","POSIX test adapters; not hardware drivers. MODULES lists providers.\n")<0) {
        fprintf(stderr, "HAL rejected\n");
        close(fd);
        return 2;
    }
    shell();
    return vol.poisoned ? 1 : 0;
}
