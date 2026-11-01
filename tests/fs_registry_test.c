/* vm2 studios | t.me/v2vproj — invalid static filesystem modules rejected. */
#include "fs_backend.h"
#include <assert.h>
#include <stdio.h>
int disk_read(u32 l,u8*b){(void)l;(void)b;return IO;}
int disk_write(u32 l,const u8*b){(void)l;(void)b;assert(0);return IO;}
int disk_flush(void){return 0;}
u32 disk_sectors(void){return 1024;}
int disk_writable(void){return 1;}
void out(const char*s){(void)s;}
void putch(char c){(void)c;}
int main(void){
 const MultiFsDriver *all[]={&multi_fs_exfat,&multi_fs_fat32,&multi_fs_ntfs};
 assert(!multi_fs_validate(all,3));assert(multi_fs_validate(0,1)==INVALID);
 assert(multi_fs_validate(all,0)==INVALID);assert(multi_fs_validate(all,17)==INVALID);
 MultiFsDriver d=multi_fs_fat32;const MultiFsDriver *one[]={&d};
 #define BAD_FIELD(field,value) do{d=multi_fs_fat32;d.field=value;assert(multi_fs_validate(one,1)==INVALID);}while(0)
 BAD_FIELD(abi,4);BAD_FIELD(abi,999);BAD_FIELD(abi,2);BAD_FIELD(size,1);BAD_FIELD(kind,-1);BAD_FIELD(id,"");
 BAD_FIELD(id,"Bad ID");BAD_FIELD(name,0);BAD_FIELD(capabilities,0);
 BAD_FIELD(capabilities,0x80000001u);BAD_FIELD(probe,0);BAD_FIELD(partition_priority,0);
 BAD_FIELD(mount,0);BAD_FIELD(root,0);BAD_FIELD(next,0);BAD_FIELD(read,0);BAD_FIELD(read64,0);BAD_FIELD(free,0);BAD_FIELD(name_equal,0);
 d=multi_fs_fat32;d.capabilities|=MULTIOS_FS_RANDOM_IO;d.write_at=0;assert(multi_fs_validate(one,1)==INVALID);
 d=multi_fs_fat32;d.capabilities|=MULTIOS_FS_RANDOM_IO;d.resize=0;assert(multi_fs_validate(one,1)==INVALID);
 d=multi_fs_fat32;d.capabilities&=~MULTIOS_FS_RANDOM_IO;d.write_at=(int(*)(const char*,uint64_t,const u8*,u32))1;assert(multi_fs_validate(one,1)==INVALID);
 d=multi_fs_fat32;d.capabilities&=~MULTIOS_FS_RANDOM_IO;d.resize=(int(*)(const char*,uint64_t))1;assert(multi_fs_validate(one,1)==INVALID);
 BAD_FIELD(capabilities,MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_NAMED_STREAMS);
 BAD_FIELD(capabilities,MULTIOS_FS_READ|MULTIOS_FS_WRITE|MULTIOS_FS_HARDLINKS);
 BAD_FIELD(link,(int(*)(const char*,const char*))1);
 BAD_FIELD(stream_next,(int(*)(const char*,u32*,MultiNamedStream*))1);
 BAD_FIELD(stream_read,(int(*)(const char*,const char*,uint64_t,u8*,u32))1);
 BAD_FIELD(stream_change,(int(*)(const char*,const char*,int,uint64_t,const u8*,u32))1);
 BAD_FIELD(unlock,0);BAD_FIELD(put,0);BAD_FIELD(remove,0);BAD_FIELD(rename,0);
 d=multi_fs_fat32;const MultiFsDriver *two[]={&multi_fs_fat32,&d};
 assert(multi_fs_validate(two,2)==INVALID);d.kind=88;assert(multi_fs_validate(two,2)==INVALID);
 d.id="other";assert(!multi_fs_validate(two,2));d.kind=FS_FAT32;assert(multi_fs_validate(two,2)==INVALID);
 assert(!(multi_fs_ntfs.capabilities&MULTIOS_FS_WRITE));
 assert(!multi_fs_ntfs.put&&!multi_fs_ntfs.remove&&!multi_fs_ntfs.rename&&!multi_fs_ntfs.unlock);
 vol.kind=FS_NTFS;vol.mounted=1;vol.writable=1;
 assert(fs_unlock()==READONLY&&!vol.writable);vol.writable=1;
 assert(fs_put("/denied",0,0,0,0)==READONLY);assert(fs_remove("/denied",0)==READONLY);assert(fs_rename("/a","/b")==READONLY);
 assert(fs_stream_next("/file",&(u32){0},&(MultiNamedStream){0})==UNSUP);
 assert(fs_stream_read("/file","meta",0,0,0)==UNSUP);
 puts("PASS: registry ABI/capability/callback validation, duplicate IDs/kinds, NTFS VFS write guard.");
}
