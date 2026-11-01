/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTIOS_MODULES_H
#define MULTIOS_MODULES_H
#include "multios/hal.h"
#define MULTIOS_MODULE_ABI 1u
#define MULTI_MODULE_KINDS 4
#define MULTI_MODULE_ID 40
#define MULTI_MODULE_OPTIONS 128
#define MULTI_CONFIG_LIMIT 2048
/* Native C descriptors, NOT an on-disk binary ABI or an ELF loader. */
enum { MULTI_OUTPUT, MULTI_INPUT, MULTI_BLOCK, MULTI_POWER };
typedef struct {
    void *ctx;
    void (*putc)(void *, char);
    void (*clear)(void *);
    void (*osk)(void *, int, int, int, int);
} MultiOutput;
typedef struct { void *ctx; int (*getc)(void *); } MultiInput;
typedef struct {
    void *ctx;
    void (*stop)(void *);
    int (*poweroff)(void *);
    int (*reboot)(void *);
} MultiPower;
typedef union {
    MultiOutput output;
    MultiInput input;
    MultiBlock block;
    MultiPower power;
} MultiDevice;
typedef struct {
    u32 abi, size;
    const char *id;
    int kind;
    const char *description;
    /* Failure must release anything acquired inside open. Successful opens
     * are closed in reverse order if a later module fails. No writes in open. */
    int (*open)(void *platform, const char *options, MultiDevice *device);
    void (*close)(MultiDevice *device); /* Optional; same ownership as kind. */
} MultiDriver;
typedef struct {
    char id[MULTI_MODULE_KINDS][MULTI_MODULE_ID];
    char options[MULTI_MODULE_KINDS][MULTI_MODULE_OPTIONS];
    int osk; /* -1: use platform default, 0: off, 1: on */
} MultiProfile;
#define MULTI_DRIVER(id_, kind_, description_, open_, close_) \
    {MULTIOS_MODULE_ABI, sizeof(MultiDriver), id_, kind_, description_, open_, close_}
/* Strict flat ASCII key=value config. Atomic parse; no heap or filesystem. */
int multi_profile_parse(MultiProfile *profile, const char *text, u32 length);
/* Validate all selections before open; open output,input,block,power; then bind.
 * Successful boot is frozen: no live unload/replacement, no silent fallback.
 * Registry, strings, platform context must outlive shell. */
int multi_modules_start(const MultiDriver *const *registry, u32 count,
                        const MultiProfile *profile, void *platform,
                        const char *name, const char *architecture, const char *inventory);
const char *multi_modules_error(void);
void multi_modules_list(void);
#endif
