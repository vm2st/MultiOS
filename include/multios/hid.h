/* MultiPulti | vm2 studios | t.me/v2vproj — USB HID boot decoder, ISA independent. */
#ifndef MULTIOS_HID_H
#define MULTIOS_HID_H
#include "multios/osk.h"
#define MULTI_HID_QUEUE 16
typedef struct { u8 previous[8],caps,num,head,tail; int queue[MULTI_HID_QUEUE]; u32 rollovers,overflows; } MultiHid;
void multi_hid_reset(MultiHid *h);
int multi_hid_report(MultiHid *h,const u8 *report,u32 length);
int multi_hid_get(MultiHid *h); /* -2: empty, not EOF. */
#endif
