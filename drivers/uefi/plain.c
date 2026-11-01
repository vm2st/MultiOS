/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
/* Minimal firmware stream output, without an OSK renderer or 80x25 layout. */
#include "platform/uefi/platform.h"
#include "unicode.h"
static MultiUtf8State decoder;
static void emit_plain(void *ctx, char c) {
    EFI_SYSTEM_TABLE *st=ctx;
    CHAR16 buf[3]={0,0,0};
    u32 scalar;int r=multi_utf8_feed(&decoder,(u8)c,&scalar);if(!r)return;
    if(r<0){decoder=(MultiUtf8State){0};scalar=(u8)c<128?(u8)c:'?';}
    if(scalar=='\n'){buf[0]='\r';buf[1]='\n';}else buf[0]=scalar>65535?'?':(CHAR16)scalar;
    uefi_call_wrapper(st->ConOut->OutputString,2,st->ConOut,buf);
}
static void clear_plain(void *ctx) {
    EFI_SYSTEM_TABLE *st=ctx;decoder=(MultiUtf8State){0};
    uefi_call_wrapper(st->ConOut->ClearScreen,1,st->ConOut);
}
static int open_plain(void *ctx,const char *options,MultiDevice *d) {
    if (*options) return INVALID;decoder=(MultiUtf8State){0};
    if (!((MultiEfi *)ctx)->st->ConOut) return UNSUP;
    d->output=(MultiOutput){.ctx=((MultiEfi *)ctx)->st,.putc=emit_plain,.clear=clear_plain};
    return 0;
}
const MultiDriver multi_uefi_plain=MULTI_DRIVER("uefi-plain",MULTI_OUTPUT,
    "minimal firmware text stream; no OSK",open_plain,0);
