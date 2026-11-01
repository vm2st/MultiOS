/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/pc-bios/io.h"
#include "unicode.h"
static MultiUtf8State decoder;
static volatile u16 *vga = (u16 *)0xb8000;
static int row, col;
static void serial(char c) {
    for (u32 i = 0; i < 1000000; i++)
        if (ib(0x3fd) & 32) {
            ob(0x3f8, c);
            return;
        }
}
static void cursor(void) {
    u16 p = row * 80 + col;
    ob(0x3d4, 14);
    ob(0x3d5, p >> 8);
    ob(0x3d4, 15);
    ob(0x3d5, p);
}
static void pc_osk(void *, int, int, int, int);
static void pc_clear(void *ctx) {
    (void)ctx;
    for (int i = 0; i < 2000; i++)
        vga[i] = 0x0720;
    row = col = 0;decoder=(MultiUtf8State){0};
    cursor();
    const char *s = "\033[2J\033[H";
    while (*s)
        serial(*s++);
    if (multi_osk_enabled()) {
        int r, c, sh;
        multi_osk_position(&r, &c, &sh);
        pc_osk(0, 1, r, c, sh);
    }
}
static void pc_putc(void *ctx, char c) {
    (void)ctx;
    if((u8)c<32)decoder=(MultiUtf8State){0};
    if (c == '\n') {
        serial('\r');
        serial('\n');
        col = 0;
        row++;
    } else if (c == '\r') {
        serial(c);
        col = 0;
    } else if (c == '\b') {
        serial(c);
        if (col)col--;else if(row){row--;col=79;}
    } else if (c == '\t') {
        do {
            putch(' ');
        } while (col % 8);
        return;
    } else {
        serial(c); /* Preserve UTF-8 on COM1, independent of VGA's limited font. */
        u32 scalar;int decoded=multi_utf8_feed(&decoder,(u8)c,&scalar);
        if(!decoded)return;
        if(decoded<0){decoder=(MultiUtf8State){0};scalar='?';}
        vga[row * 80 + col++] = 0x0700 | (scalar<128?scalar:'?');
        if (col == 80) {
            col = 0;
            row++;
        }
    }
    int rows = multi_osk_enabled() ? 18 : 25;
    if (row >= rows) {
        for (int i = 0; i < (rows - 1) * 80; i++)
            vga[i] = vga[i + 80];
        for (int i = (rows - 1) * 80; i < rows * 80; i++)
            vga[i] = 0x0720;
        row = rows - 1;
    }
    cursor();
}
static void paneltext(int x, int y, const char *s, u16 a) {
    while (*s && x < 80)
        vga[y * 80 + x++] = a | (u8)*s++;
}
static void pc_osk(void *ctx, int on, int selected_row, int selected_col, int shift) {
    (void)ctx;
    for (int y = 18; y < 25; y++)
        for (int x = 0; x < 80; x++)
            vga[y * 80 + x] = 0x0720;
    if (on) {
        if (row >= 18) {
            row = 17;
            col = 0;
        }
        paneltext(1, 18, "OSK | F6: Next | F7: Select | F5: Previous (optional)", 0x1f00);
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < multi_osk_columns(r); c++) {
                u16 a = (r == selected_row && c == selected_col) ? 0x7000 : 0x1f00;
                for (int x = 0; x < 6; x++)
                    vga[(19 + r) * 80 + 2 + c * 6 + x] = a | ' ';
                paneltext(2 + c * 6, 19 + r, multi_osk_label(r, c, shift), a);
            }
        paneltext(1, 24, "Hide: close | F6/F7: reopen | arrows + Enter also work", 0x1f00);
    }
    if (!on)
        paneltext(1, 24, "OSK hidden: F6 or F7 opens; Up / F2 / Ctrl+O also work", 0x1f00);
    cursor();
}
static int open_text(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx; if (*options) return INVALID;
    ob(0x3f9, 0);
    ob(0x3fb, 0x80);
    ob(0x3f8, 1);
    ob(0x3f9, 0);
    ob(0x3fb, 3);
    ob(0x3fa, 0xc7);
    ob(0x3fc, 0x0b);
    for (int i = 0; i < 2000; i++)
        vga[i] = 0x0720;
    row=col=0;decoder=(MultiUtf8State){0};
    d->output=(MultiOutput){.putc=pc_putc,.clear=pc_clear,.osk=pc_osk}; return 0;
}
const MultiDriver multi_pc_text=MULTI_DRIVER("pc-vga-com1",MULTI_OUTPUT,
    "native VGA 80x25 + COM1 mirror + OSK",open_text,0);
