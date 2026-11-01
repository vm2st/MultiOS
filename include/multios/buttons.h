/* vm2 studios | t.me/v2vproj — minimal portable two-button adapter. */
#ifndef MULTIOS_BUTTONS_H
#define MULTIOS_BUTTONS_H
#include "multios/osk.h"
enum { MULTI_BUTTON_NEXT = 1, MULTI_BUTTON_SELECT = 2, MULTI_BUTTON_PREVIOUS = 4 };
typedef struct { unsigned char latched, has_previous; } MultiButtons;
/* Zero initialization means exactly two buttons. Call once before sampling;
 * 3+ physical buttons expose optional PREVIOUS. Not for live reconfiguration. */
static inline int multi_buttons_init(MultiButtons *b, unsigned count) {
    if (!b || count < 2) return INVALID;
    b->latched = 0;
    b->has_previous = count >= 3;
    return 0;
}
/* Feed a debounced logical bitmask (NEXT=1, SELECT=2, PREVIOUS=4), never raw bouncing GPIO.
 * One event per press. Release ALL buttons to rearm. A chord is ignored and latched.
 * -2 means no event. The BSP supplies electrical polarity, debounce and wait.
 * No timer/CPU/IRQ/GPIO assumptions in this adapter. Zero-initialize state. */
static inline int multi_buttons_update(MultiButtons *b, unsigned levels) {
    if (!b || levels > 7) return -2;
    if (!levels) { b->latched = 0; return -2; }
    if (b->latched) return -2;
    b->latched = 1;
    if (levels & ~(b->has_previous ? 7u : 3u)) return -2;
    if (levels == MULTI_BUTTON_NEXT) return MULTI_KEY_NEXT;
    if (levels == MULTI_BUTTON_SELECT) return MULTI_KEY_SELECT;
    if (b->has_previous && levels == MULTI_BUTTON_PREVIOUS) return MULTI_KEY_PREVIOUS;
    return -2;
}
#endif
