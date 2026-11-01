/* SPDX-License-Identifier: GPL-2.0-or-later
 * vm2 studios | t.me/v2vproj — expose upstream clean-log check unchanged.
 */
#include "../../vendor/ntfs-3g/libntfs-3g/volume.c"
int mpntfs_check_clean(ntfs_volume*v){return ntfs_volume_check_logfile(v);}
