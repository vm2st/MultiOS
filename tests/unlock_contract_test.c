/* Malfunctioning module must not turn a failed unlock into VFS write access. */
#include "fs_backend.h"
#include <assert.h>
#include <stdio.h>
static int scenario,provider=1,binding=1,calls;
int disk_writable(void){return provider;}
int multi_storage_matches(u32 b,u32 n){return binding&&b==2048&&n==100;}
static int unlock(void){calls++;vol.writable=1;
 switch(scenario){case 1:return IO;case 2:return 1;case 3:vol.poisoned=1;break;case 4:vol.mounted=0;break;
 case 5:vol.writable=0;break;case 6:vol.kind=FS_NTFS;break;case 7:vol.base++;break;
 case 8:vol.sectors--;break;case 9:provider=0;break;case 10:binding=0;break;}
 return 0;
}
const MultiFsDriver multi_fs_fat32={.kind=FS_FAT32,.capabilities=MULTIOS_FS_READ|MULTIOS_FS_WRITE,.unlock=unlock};
int main(void){
 for(scenario=0;scenario<=10;scenario++){
  vol=(Volume){.mounted=1,.kind=FS_FAT32,.base=2048,.sectors=100};provider=binding=1;calls=0;
  int r=fs_unlock();assert(calls==1);
  if(scenario)assert(r<0&&!vol.writable);else assert(!r&&vol.writable);
 }
 vol=(Volume){.mounted=1,.kind=FS_FAT32,.base=2048,.sectors=100};provider=0;binding=1;calls=0;
 assert(fs_unlock()==READONLY&&!vol.writable&&!calls);
 puts("PASS VFS unlock contract: valid success, negative/positive failure, poisoned/unmounted/RO view, changed driver/geometry, lost provider/binding, initial readonly provider.");
}
