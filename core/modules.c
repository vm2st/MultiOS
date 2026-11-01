/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "multios/modules.h"
#include "multios/brand.h"
static const char *const names[] = {"output", "input", "block", "power"};
static const char *error = "none";
static const MultiDriver *selected[MULTI_MODULE_KINDS];
static MultiDevice devices[MULTI_MODULE_KINDS];
static int started;
static MultiProfile active_profile;
static const MultiDriver *const *available;
static u32 available_count;
static int valid_id(const char *s) {
    u32 n = 0;
    if (!s) return 0;
    for (; n < MULTI_MODULE_ID && s[n]; n++) {
        char c = s[n];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return 0;
    }
    return n && n < MULTI_MODULE_ID;
}
int multi_profile_parse(MultiProfile *p, const char *text, u32 length) {
    if (!p || !text || length > MULTI_CONFIG_LIMIT) return INVALID;
    MultiProfile draft = *p;
    u32 seen = 0, pos = 0;
    while (pos < length) {
        char line[180]; u32 n = 0;
        while (pos < length && text[pos] != '\n') {
            unsigned char c = text[pos++];
            if ((c < 32 && c != '\r' && c != '\t') || c > 126 || n >= sizeof(line)-1) return INVALID;
            line[n++] = (char)c;
        }
        if (pos < length) pos++;
        while (n && (line[n-1] == ' ' || line[n-1] == '\t' || line[n-1] == '\r')) n--;
        line[n] = 0;
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        if (!*s || *s == '#' || *s == ';') continue;
        char *eq = s; while (*eq && *eq != '=') eq++;
        if (!*eq) return INVALID;
        *eq++ = 0;
        char *end = eq-1;
        while (end > s && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;
        while (*eq == ' ' || *eq == '\t') eq++;
        int field = -1;
        for (int k = 0; k < MULTI_MODULE_KINDS; k++) {
            if (streq(s, names[k])) field = k;
            char key[24]; u32 j=0;
            while (names[k][j]) { key[j] = names[k][j]; j++; }
            mcopy(key+j, ".options", 9);
            if (streq(s, key)) field = k+4;
        }
        if (streq(s, "osk")) field = 8;
        if (field < 0 || (seen & (1u << field))) return INVALID;
        seen |= 1u << field;
        if (field == 8) {
            if (streq(eq, "on")) draft.osk = 1;
            else if (streq(eq, "off")) draft.osk = 0;
            else if (streq(eq, "auto")) draft.osk = -1;
            else return INVALID;
        } else {
            u32 len = slen(eq), cap = field < 4 ? MULTI_MODULE_ID : MULTI_MODULE_OPTIONS;
            if (len >= cap || (field < 4 && !valid_id(eq))) return INVALID;
            char *dest = field < 4 ? draft.id[field] : draft.options[field-4];
            mcopy(dest, eq, len+1);
        }
    }
    *p = draft;
    return 0;
}
static void emit(void *ctx, char ch) { (void)ctx; devices[MULTI_OUTPUT].output.putc(devices[MULTI_OUTPUT].output.ctx, ch); }
static int getkey(void *ctx) { (void)ctx; return devices[MULTI_INPUT].input.getc(devices[MULTI_INPUT].input.ctx); }
static void clear(void *ctx) { (void)ctx; devices[MULTI_OUTPUT].output.clear(devices[MULTI_OUTPUT].output.ctx); }
static void osk(void *ctx, int on, int row, int col, int shift) {
    (void)ctx; devices[MULTI_OUTPUT].output.osk(devices[MULTI_OUTPUT].output.ctx, on, row, col, shift);
}
static int valid_device(int k, MultiDevice *d) {
    if (k == MULTI_OUTPUT) return d->output.putc != 0;
    if (k == MULTI_INPUT) return d->input.getc != 0;
    if (k == MULTI_POWER) return d->power.stop != 0;
    return d->block.read && d->block.sectors && d->block.sectors <= 0x10000000u &&
           d->block.sector_bytes == 512 &&
           (!d->block.writable || (d->block.write && d->block.flush));
}
int multi_modules_start(const MultiDriver *const *registry, u32 count,
                        const MultiProfile *p, void *platform, const char *name,
                        const char *architecture, const char *inventory) {
    if (started || vol.mounted || multi_hal()) { error="already bound"; return INVALID; }
    if (!registry || !count || count > 64 || !p || !name || !architecture) { error="invalid arguments"; return INVALID; }
    mzero(selected, sizeof(selected));
    for (u32 i=0; i<count; i++) {
        const MultiDriver *d=registry[i];
        if (!d || d->abi != MULTIOS_MODULE_ABI || d->size != sizeof(MultiDriver) ||
            !valid_id(d->id) || d->kind < 0 || d->kind >= 4 || !d->open || !d->description) {
            error="registry ABI/descriptor mismatch"; return INVALID;
        }
        for (u32 j=0; j<i; j++) if (streq(d->id, registry[j]->id)) { error="duplicate module ID"; return INVALID; }
    }
    for (int k=0; k<4; k++) {
        if (!valid_id(p->id[k])) { error=names[k]; return INVALID; }
        /* Options must terminate even when profile was not built by parser. */
        u32 n=0; while (n<MULTI_MODULE_OPTIONS && p->options[k][n]) n++;
        if (n==MULTI_MODULE_OPTIONS) { error="unterminated options"; return INVALID; }
        for (u32 i=0; i<count; i++) if (streq(registry[i]->id, p->id[k]) && registry[i]->kind==k) selected[k]=registry[i];
        if (!selected[k]) { error=names[k]; return NOTFOUND; }
    }
    if (p->osk < -1 || p->osk > 1) { error="invalid osk"; return INVALID; }
    mzero(devices, sizeof(devices));
    int opened=0, result=INVALID;
    for (int k=0; k<4; k++) {
        error=selected[k]->id;
        result=selected[k]->open(platform, p->options[k], &devices[k]);
        if (result < 0) goto fail;
        opened++;
        if (result != 0 || !valid_device(k, &devices[k])) { result=INVALID; goto fail; }
    }
    if (p->osk == 1 && !devices[0].output.osk) { error="output has no OSK renderer"; result=UNSUP; goto fail; }
    MultiHal h={.abi=MULTIOS_HAL_ABI, .name=name, .architecture=architecture,
        .block=devices[MULTI_BLOCK].block,
        .console={.putc=emit, .getc=getkey,
          .clear=devices[0].output.clear ? clear : 0, .osk=devices[0].output.osk ? osk : 0},
        .lifecycle_ctx=devices[MULTI_POWER].power.ctx,
        .stop=devices[MULTI_POWER].power.stop,
        .poweroff=devices[MULTI_POWER].power.poweroff,
        .reboot=devices[MULTI_POWER].power.reboot, .drivers=inventory};
    result=multi_hal_bind(&h);
    if (result < 0) { error="HAL bind rejected"; goto fail; }
    active_profile=*p;available=registry;available_count=count;
    started=1; error="none";
    return 0;
fail:
    while (opened) { int k=--opened; if (selected[k]->close) selected[k]->close(&devices[k]); }
    mzero(devices,sizeof(devices));
    return result;
}
const char *multi_modules_error(void) { return error; }
void multi_modules_list(void) {
    if (!started) { out("Legacy direct HAL binding; no module registry active.\n"); return; }
    out(MULTIOS_CREDIT "\nModule ABI 1 | boot-selected static modules | no live unload\n");
    for (int k=0; k<4; k++) {
        out(names[k]); out(": "); out(selected[k]->id); out(" - "); out(selected[k]->description); out("\n");
        if(active_profile.options[k][0]){out("  options: ");out(active_profile.options[k]);out("\n");}
    }
    out("Available compiled modules (replacement requires restart):\n");
    for(u32 i=0;i<available_count;i++){out("  ");out(names[available[i]->kind]);out("=");out(available[i]->id);out("\n");}

}
