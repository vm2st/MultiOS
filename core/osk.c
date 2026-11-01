/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "multios/osk.h"
#include "multios/hal.h"
static int enabled, row, col, shift;
static void redraw(void) {
    const MultiHal *h = multi_hal();
    if (h && h->console.osk)
        h->console.osk(h->console.ctx, enabled, row, col, shift);
}
int multi_osk_enabled(void) {
    return enabled;
}
int multi_osk_enable(int on) {
    const MultiHal *h = multi_hal();
    if (on && (!h || !h->console.osk))
        return UNSUP;
    enabled = !!on;
    redraw();
    return 0;
}
int multi_osk_columns(int r) {
    return r == 4 ? 8 : 12;
}
void multi_osk_position(int *r, int *c, int *s) {
    if (r)
        *r = row;
    if (c)
        *c = col;
    if (s)
        *s = shift;
}
const char *multi_osk_label(int r, int c, int sh) {
    static char ch[2];
    static const char *normal[] = {"1234567890-=", "qwertyuiop[]", "asdfghjkl;'`", "zxcvbnm,./\\~"};
    static const char *upper[] = {"!@#$%^&*()_+", "QWERTYUIOP{}", "ASDFGHJKL:\"~", "ZXCVBNM<>?|`"};
    static const char *actions[] = {"Space", "Back", "Enter", "Shift",
                                    "Hide",  "Tab",  "Esc",   "CtrlC"};
    if (r < 0 || r > 4 || c < 0 || c >= multi_osk_columns(r))
        return "";
    if (r == 4)
        return actions[c];
    ch[0] = (sh ? upper[r] : normal[r])[c];
    ch[1] = 0;
    return ch;
}
int multi_osk_process(int key) {
    if (key == MULTI_KEY_NEXT || key == MULTI_KEY_SELECT || key == MULTI_KEY_PREVIOUS) {
        if (!enabled) {
            /* First press only opens; preserves selection and shift. */
            multi_osk_enable(1);
            return -2;
        }
        if (key == MULTI_KEY_PREVIOUS) {
            if (--col < 0) {
                row = (row + 4) % 5;
                col = multi_osk_columns(row) - 1;
            }
            redraw();
            return -2;
        }
        if (key == MULTI_KEY_NEXT) {
            if (++col >= multi_osk_columns(row)) {
                col = 0;
                row = (row + 1) % 5;
            }
            redraw();
            return -2;
        }
        key = '\n';
    }
    if (key == MULTI_KEY_OSK || key == 15) {
        multi_osk_enable(!enabled);
        return -2;
    }
    if (!enabled) {
        if (key == MULTI_KEY_UP) {
            multi_osk_enable(1);
            return -2;
        }
        if(key==MULTI_KEY_LEFT||key==MULTI_KEY_RIGHT||key==MULTI_KEY_HOME||key==MULTI_KEY_END||key==MULTI_KEY_DELETE||key==MULTI_KEY_HISTORY_PREV||key==MULTI_KEY_HISTORY_NEXT)return key;
        if (key >= 256)
            return -2;
        return key;
    }
    if (key == 27) {
        multi_osk_enable(0);
        return -2;
    }
    if (key >= MULTI_KEY_UP && key <= MULTI_KEY_RIGHT) {
        if (key == MULTI_KEY_UP)
            row = (row + 4) % 5;
        if (key == MULTI_KEY_DOWN)
            row = (row + 1) % 5;
        int cols = multi_osk_columns(row);
        if (col >= cols)
            col = cols - 1;
        if (key == MULTI_KEY_LEFT)
            col = (col + cols - 1) % cols;
        if (key == MULTI_KEY_RIGHT)
            col = (col + 1) % cols;
        redraw();
        return -2;
    }
    if (key == '\r' || key == '\n') {
        if (row < 4)
            return (unsigned char)multi_osk_label(row, col, shift)[0];
        switch (col) {
        case 0:
            return ' ';
        case 1:
            return 8;
        case 2:
            return '\n';
        case 3:
            shift = !shift;
            redraw();
            return -2;
        case 4:
            multi_osk_enable(0);
            return -2;
        case 5:
            return '\t';
        case 6:
            multi_osk_enable(0);
            return -2;
        case 7:
            return 3;
        }
    }
    return key;
}
