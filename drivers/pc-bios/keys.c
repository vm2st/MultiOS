/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/pc-bios/io.h"
#include "platform/pc-bios/usb.h"
static int usb_only,ps2_only;
/* US set-1 PS/2 keyboard. Polling: no IRQ/IDT scheduler in this prototype. */
static int pc_getc(void *ctx) {
    (void)ctx;
    static int shift, ctrl, caps, num, extended, caps_down, num_down;
    static int skip_lf,pause_bytes;
    static const char normal[128] = {
        [1] = 27,    [2] = '1',  [3] = '2',  [4] = '3',   [5] = '4',  [6] = '5',   [7] = '6',
        [8] = '7',   [9] = '8',  [10] = '9', [11] = '0',  [12] = '-', [13] = '=',  [14] = 8,
        [15] = '\t', [16] = 'q', [17] = 'w', [18] = 'e',  [19] = 'r', [20] = 't',  [21] = 'y',
        [22] = 'u',  [23] = 'i', [24] = 'o', [25] = 'p',  [26] = '[', [27] = ']',  [28] = '\n',
        [30] = 'a',  [31] = 's', [32] = 'd', [33] = 'f',  [34] = 'g', [35] = 'h',  [36] = 'j',
        [37] = 'k',  [38] = 'l', [39] = ';', [40] = '\'', [41] = '`', [43] = '\\', [44] = 'z',
        [45] = 'x',  [46] = 'c', [47] = 'v', [48] = 'b',  [49] = 'n', [50] = 'm',  [51] = ',',
        [52] = '.',  [53] = '/', [57] = ' '};
    static const char shifted[128] = {
        [2] = '!',  [3] = '@',  [4] = '#',  [5] = '$',  [6] = '%',  [7] = '^',  [8] = '&',
        [9] = '*',  [10] = '(', [11] = ')', [12] = '_', [13] = '+', [26] = '{', [27] = '}',
        [39] = ':', [40] = '"', [41] = '~', [43] = '|', [51] = '<', [52] = '>', [53] = '?'};
    for (;;) {
        int usb=ps2_only?-2:multi_usb_key();if(usb!=-2)return usb;
        if(usb_only)continue;
        if (ib(0x3fd) & 1) {
            int c = ib(0x3f8);
            if (c == '\n' && skip_lf) {
                skip_lf = 0;
                continue;
            }
            skip_lf = c == '\r';
            return c;
        }
        u8 status = ib(0x64);
        if (!(status & 1))
            continue;
        u8 s = ib(0x60);
        if (status & 0x20)
            continue;
        if(pause_bytes){pause_bytes--;continue;}
        if(s==0xe1){pause_bytes=5;continue;}
        if (s == 0xe0) {
            extended = 1;
            continue;
        }
        if (extended) {
            extended = 0;
            if((s&127)==29){if(s&128)ctrl&=~2;else ctrl|=2;continue;}
            if (s & 128)
                continue;
            if(s==28)return '\n';if(s==53)return '/';
            if (s == 71)return MULTI_KEY_HOME;
            if (s == 79)return MULTI_KEY_END;
            if (s == 83)return MULTI_KEY_DELETE;
            if (s == 72)
                return MULTI_KEY_UP;
            if (s == 80)
                return MULTI_KEY_DOWN;
            if (s == 75)
                return MULTI_KEY_LEFT;
            if (s == 77)
                return MULTI_KEY_RIGHT;
            continue;
        }
        if(s==66)return MULTI_KEY_HISTORY_PREV; /* F8 */
        if(s==67)return MULTI_KEY_HISTORY_NEXT; /* F9 */
        if (s == 63) return MULTI_KEY_PREVIOUS; /* F5: optional third button */
        if (s == 64) return MULTI_KEY_NEXT;   /* F6: two-button NEXT */
        if (s == 65) return MULTI_KEY_SELECT; /* F7: two-button SELECT */
        if (s == 60)
            return MULTI_KEY_OSK;
        if ((s & 0x7f) == 42 || (s & 0x7f) == 54) {
            int mask=(s&127)==42?1:2;
            if(s&128)shift&=~mask;else shift|=mask;
            continue;
        }
        if ((s & 0x7f) == 29) {
            if(s&128)ctrl&=~1;else ctrl|=1;
            continue;
        }
        if((s&127)==58){if(s&128)caps_down=0;else if(!caps_down){caps^=1;caps_down=1;}continue;}
        if((s&127)==69){if(s&128)num_down=0;else if(!num_down){num^=1;num_down=1;}continue;}
        if (s & 0x80)continue;
        if(s==55)return '*';if(s==74)return '-';if(s==78)return '+';
        if(s>=71&&s<=83){
            static const char digits[13]="789-456+1230.";
            if(num)return digits[s-71];
            switch(s){case 71:return MULTI_KEY_HOME;case 72:return MULTI_KEY_UP;
            case 75:return MULTI_KEY_LEFT;case 77:return MULTI_KEY_RIGHT;
            case 79:return MULTI_KEY_END;case 80:return MULTI_KEY_DOWN;case 83:return MULTI_KEY_DELETE;}
            continue;
        }
        char c = normal[s];
        if (!c)
            continue;
        if (ctrl && c >= 'a' && c <= 'z')
            return c - 'a' + 1;
        if (c >= 'a' && c <= 'z') {
            if (!!shift ^ caps)
                c -= 32;
        } else if (shift && shifted[s])
            c = shifted[s];
        return c;
    }
}
static int open_keys(void *ctx,const char *options,MultiDevice *d) {
    (void)ctx;
    if(*options&&!streq(options,"auto")&&!streq(options,"usb-only")&&!streq(options,"ps2-only"))return INVALID;
    usb_only=streq(options,"usb-only");ps2_only=streq(options,"ps2-only");
    if(!streq(options,"ps2-only"))multi_usb_init();
    d->input=(MultiInput){.getc=pc_getc}; return 0;
}
static void close_keys(MultiDevice *d){(void)d;multi_usb_stop();}
const MultiDriver multi_pc_keys=MULTI_DRIVER("pc-input-auto",MULTI_INPUT,
    "native PS2 + COM1 + UHCI/OHCI/EHCI boot USB keyboard polling",open_keys,close_keys);
