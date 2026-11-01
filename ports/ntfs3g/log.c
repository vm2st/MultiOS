/* SPDX-License-Identifier: GPL-2.0-or-later
 * vm2 studios | t.me/v2vproj — no stdio/syslog dependency in firmware.
 * Diagnostics retain upstream format strings; errno and return errors unchanged.
 */
#include "logging.h"
extern void out(const char*);
int ntfs_log_redirect(const char*f,const char*p,int l,u32 level,void*d,const char*fmt,...){
 (void)f;(void)p;(void)l;(void)d;
 if(level&(NTFS_LOG_LEVEL_ERROR|NTFS_LOG_LEVEL_PERROR|NTFS_LOG_LEVEL_CRITICAL)){
  out("[NTFS-3G] ");out(fmt);
 }
 return 0;
}
