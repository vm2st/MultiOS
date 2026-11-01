/* MultiPulti | vm2 studios | t.me/v2vproj — portable bounded line editor. */
#ifndef MULTIOS_EDITOR_H
#define MULTIOS_EDITOR_H
#include "multios/osk.h"
#define MULTI_EDIT_CAPACITY 512
enum { MULTI_EDIT_CHANGED=1,MULTI_EDIT_MOVED=2,MULTI_EDIT_SUBMIT=4,MULTI_EDIT_CANCEL=8 };
typedef struct {char text[MULTI_EDIT_CAPACITY];u32 length,cursor;int overflow;char pending[5];u32 pending_bytes;} MultiEdit;
void multi_edit_reset(MultiEdit *e);
int multi_edit_load(MultiEdit *e,const char *s);
int multi_edit_key(MultiEdit *e,int key);
#endif
