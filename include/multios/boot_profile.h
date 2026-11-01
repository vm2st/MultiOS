/* vm2 studios | t.me/v2vproj */
#ifndef MULTIOS_BOOT_PROFILE_H
#define MULTIOS_BOOT_PROFILE_H
#include "multios/modules.h"
/* 1 = empty fallback, 0 = valid, negative = invalid. Atomic; no I/O. */
int multi_boot_profile(MultiProfile *,const unsigned char[512]);
#endif
