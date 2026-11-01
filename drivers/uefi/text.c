/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/uefi/platform.h"
#include "unicode.h"
static EFI_SYSTEM_TABLE *system_table;
static int x, y;
static CHAR16 cells[25][80];
static MultiUtf8State decoder;
static void at(int col, int row) {
    uefi_call_wrapper(system_table->ConOut->SetCursorPosition, 3, system_table->ConOut, (UINTN)col,
                      (UINTN)row);
}
static void attr(int selected) {
    uefi_call_wrapper(system_table->ConOut->SetAttribute, 2, system_table->ConOut,
                      (UINTN)(selected ? 0x70 : 0x07));
}
static void printwide(int col,int row,const CHAR16 *s,int selected){
 at(col,row);attr(selected);uefi_call_wrapper(system_table->ConOut->OutputString,2,system_table->ConOut,(CHAR16 *)s);
}
static void printat(int col, int row, const char *s, int selected) {
    CHAR16 buf[81];
    int n = 0;
    while (*s && n < 79 - col)
        buf[n++] = (unsigned char)*s++;
    buf[n] = 0;
    at(col, row);
    attr(selected);
    uefi_call_wrapper(system_table->ConOut->OutputString, 2, system_table->ConOut, buf);
}
static void uefi_osk(void *ctx, int on, int sr, int sc, int sh) {
    (void)ctx;
    if (on && y >= 18) {
        int delta = y - 17;
        for (int r = 0; r < 18; r++)
            mcopy(cells[r], cells[r + delta], sizeof(cells[r]));
        y = 17; /* Preserve x and the current editor line. */
        for (int r = 0; r < 18; r++) {
            CHAR16 line[80];
            for (int i = 0; i < 79; i++)
                line[i] = cells[r][i] ? cells[r][i] : ' ';
            line[79] = 0;
            printwide(0, r, line, 0);
        }
    }
    for (int r = 18; r < 25; r++)
        mzero(cells[r], sizeof(cells[r]));
    char blank[80];
    for (int i = 0; i < 79; i++)
        blank[i] = ' ';
    blank[79] = 0;
    for (int r = 18; r < 25; r++)
        printat(0, r, blank, 0);
    if (on) {
        printat(1, 18, "OSK | F6: Next | F7: Select | F5: Previous (optional)", 0);
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < multi_osk_columns(r); c++) {
                printat(2 + c * 6, 19 + r, "      ", r == sr && c == sc);
                printat(2 + c * 6, 19 + r, multi_osk_label(r, c, sh), r == sr && c == sc);
            }
        printat(1, 24, "Hide: close | F6/F7: reopen | arrows + Enter also work", 0);
    }
    if (!on)
        printat(1, 24, "OSK hidden: F6 or F7 opens; Up / F2 / Ctrl+O also work", 0);
    attr(0);
    at(x, y);
}
static void clear(void *ctx) {
    (void)ctx;
    uefi_call_wrapper(system_table->ConOut->ClearScreen, 1, system_table->ConOut);
    mzero(cells, sizeof(cells));
    x = y = 0;decoder=(MultiUtf8State){0};
    if (multi_osk_enabled()) {
        int r, c, s;
        multi_osk_position(&r, &c, &s);
        uefi_osk(0, 1, r, c, s);
    }
}
static void emit(void *ctx, char c) {
    (void)ctx;
    u32 scalar;int decoded=multi_utf8_feed(&decoder,(u8)c,&scalar);
    if(decoded==0)return;
    if(decoded<0){decoder=(MultiUtf8State){0};if((u8)c<128)scalar=(u8)c;else scalar='?';}
    if (scalar == '\n') {
        x = 0;
        y++;
    } else if (scalar == '\r') {
        x = 0;
    } else if (scalar == '\b') {
        if (x)x--;else if(y){y--;x=78;}
    } else if (scalar == '\t') {
        do {
            emit(0, ' ');
        } while (x % 8);
        return;
    } else {
        CHAR16 ch=scalar>65535?'?':(CHAR16)scalar;
        cells[y][x]=ch;CHAR16 s[2]={ch,0};printwide(x,y,s,0);
        if (++x == 79) {
            x = 0;
            y++;
        }
    }
    int rows =
        multi_osk_enabled() ? 18 : 24; /* never write bottom-right cell: firmware auto-scrolls */
    if (y >= rows) {
        for (int r = 0; r < rows - 1; r++)
            mcopy(cells[r], cells[r + 1], sizeof(cells[r]));
        mzero(cells[rows - 1], sizeof(cells[0]));
        y = rows - 1;
        for (int r = 0; r < rows; r++) {
            CHAR16 line[80];
            for (int i = 0; i < 79; i++)
                line[i] = cells[r][i] ? cells[r][i] : ' ';
            line[79] = 0;
            printwide(0, r, line, 0);
        }
    }
    at(x, y);
}
static int open_text(void *ctx, const char *options, MultiDevice *d) {
    if (*options) return INVALID;
    MultiEfi *p = ctx; EFI_SYSTEM_TABLE *st = p->st; system_table = st;
    if (!st->ConOut || !st->ConOut->Mode) return UNSUP;
    /* Prefer a text mode with at least 80x25; OSK is positioned in that rectangle. */
    int good = 0;
    for (INT32 mode = 0; mode < st->ConOut->Mode->MaxMode; mode++) {
        UINTN cols = 0, rows = 0;
        if (!EFI_ERROR(uefi_call_wrapper(st->ConOut->QueryMode, 4, st->ConOut, (UINTN)mode, &cols,
                                         &rows)) &&
            cols >= 80 && rows >= 25) {
            if (EFI_ERROR(uefi_call_wrapper(st->ConOut->SetMode, 2, st->ConOut, (UINTN)mode))) continue;
            good = 1;
            break;
        }
    }
    if (!good) {
        
        return UNSUP;
    }
    mzero(cells, sizeof(cells)); x=y=0;decoder=(MultiUtf8State){0};
    d->output=(MultiOutput){.putc=emit,.clear=clear,.osk=uefi_osk};
    return 0;
}
const MultiDriver multi_uefi_text=MULTI_DRIVER("uefi-text", MULTI_OUTPUT,
    "firmware text 80x25 + OSK panel", open_text, 0);
