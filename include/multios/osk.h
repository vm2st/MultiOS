/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#ifndef MULTIOS_OSK_H
#define MULTIOS_OSK_H
#include "multi.h"
enum { MULTI_KEY_UP = 256, MULTI_KEY_DOWN, MULTI_KEY_LEFT, MULTI_KEY_RIGHT, MULTI_KEY_OSK,
       MULTI_KEY_NEXT, MULTI_KEY_SELECT, MULTI_KEY_PREVIOUS,
       MULTI_KEY_HOME, MULTI_KEY_END, MULTI_KEY_DELETE, MULTI_KEY_HISTORY_PREV, MULTI_KEY_HISTORY_NEXT, MULTI_KEY_INPUT_LOST };
/* INPUT_LOST: a dropped input batch invalidates the current command line.
 * NEXT: cycle all cells row-major; SELECT: activate. Either opens a hidden
 * panel without emitting a character. PREVIOUS is optional for 3+ buttons;
 * it scans backward over the same cells, and also opens a hidden panel. No third key or press timing required. */
int multi_osk_enable(int enabled);
int multi_osk_enabled(void);
/* Process one raw key. -2 means consumed; otherwise character or EOF. */
int multi_osk_process(int key);
const char *multi_osk_label(int row, int col, int shift);
int multi_osk_columns(int row);
void multi_osk_position(int *row, int *col, int *shift);
#endif
