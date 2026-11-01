#include "multios/hal.h"
#include "multios/osk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;
static int paints;
static int readstub(void *c, u32 l, u8 *b) {
    (void)c;
    (void)l;
    (void)b;
    return IO;
}
static void emit(void *c, char x) {
    (void)c;
    (void)x;
}
static int key(void *c) {
    (void)c;
    return -1;
}
static void panel(void *c, int on, int r, int col, int sh) {
    (void)c;
    (void)on;
    (void)r;
    (void)col;
    (void)sh;
    paints++;
}
static void choose(int tr, int tc) {
    int r, c, s;
    multi_osk_position(&r, &c, &s);
    while (r != tr) {
        assert(multi_osk_process(MULTI_KEY_DOWN) == -2);
        multi_osk_position(&r, &c, &s);
    }
    while (c != tc) {
        assert(multi_osk_process(MULTI_KEY_RIGHT) == -2);
        multi_osk_position(&r, &c, &s);
    }
}
int main(void) {
    MultiHal h = {.abi = MULTIOS_HAL_ABI,
                  .name = "OSK test",
                  .architecture = "native",
                  .block = {.sectors = 1, .sector_bytes = 512, .read = readstub},
                  .console = {.putc = emit, .getc = key, .osk = panel}};
    assert(multi_hal_bind(&h) == 0);
    assert(!multi_osk_enabled());
    int before=paints;assert(multi_osk_enable(0)==0&&paints==before);
    assert(multi_osk_process('a') == 'a');
    assert(multi_osk_process(MULTI_KEY_UP) == -2);
    assert(multi_osk_enable(1) == 0);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 12; c++) {
            choose(r, c);
            const char *label = multi_osk_label(r, c, 0);
            int expected = (unsigned char)label[0];
            assert(multi_osk_process('\n') == expected);
        }
    choose(4, 3);
    assert(multi_osk_process('\n') == -2);
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 12; c++) {
            choose(r, c);
            int expected = (unsigned char)multi_osk_label(r, c, 1)[0];
            assert(multi_osk_process('\n') == expected);
        }
    choose(4, 0);
    assert(multi_osk_process('\n') == ' ');
    choose(4, 1);
    assert(multi_osk_process('\n') == 8);
    choose(4, 2);
    assert(multi_osk_process('\n') == '\n');
    choose(4, 7);
    assert(multi_osk_process('\n') == 3);
    choose(4, 4);
    assert(multi_osk_process('\n') == -2 && !multi_osk_enabled());
    assert(multi_osk_process(MULTI_KEY_OSK) == -2 && multi_osk_enabled());
    assert(multi_osk_process(15) == -2 && !multi_osk_enabled());
    multi_osk_enable(1);
    assert(multi_osk_process(27) == -2 && !multi_osk_enabled());
    assert(multi_osk_process(MULTI_KEY_UP) == -2 && multi_osk_enabled());
    choose(4, 4);
    assert(multi_osk_process('\n') == -2 && !multi_osk_enabled());
    assert(multi_osk_process(MULTI_KEY_UP) == -2 && multi_osk_enabled());
    multi_osk_enable(0);
    assert(paints > 100);
    h.console.osk = 0;
    assert(multi_hal_bind(&h) == 0);
    assert(multi_osk_enable(1) == UNSUP);
    puts("OSK tests passed: 96 printable grid selections, actions, shift, navigation, toggle and "
         "missing renderer");
    return 0;
}
