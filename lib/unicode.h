/* vm2 studios | t.me/v2vproj — strict bounded Unicode, no locale/heap/runtime. */
#ifndef MULTIOS_UNICODE_H
#define MULTIOS_UNICODE_H
#include "multi.h"
typedef int (*MultiUpcase)(u16,u16 *);
int multi_utf8_to16(const char *,u16 *,u32,u32 *);
int multi_utf16_to8(const u16 *,u32,char *,u32);
int multi_name_entry(const u16 *,u32,Entry *);
int multi_name_valid(const char *);
int multi_stream_name_valid(const char *);
u32 multi_name_units(const char *);
int multi_name_equal(const char *,const char *,MultiUpcase);
int multi_default_upcase(u16,u16 *);
int multi_default_equal(const char *,const char *);
/* Streaming UTF-8 -> scalar decoder; 1 complete, 0 pending, negative malformed. */
typedef struct {u32 value,minimum;u8 left;} MultiUtf8State;
int multi_utf8_feed(MultiUtf8State *,u8,u32 *);
#endif
