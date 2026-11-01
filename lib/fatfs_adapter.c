/* vm2 studios | t.me/v2vproj — FatFs HAL/VFS bridge. No host OS or FUSE.
 * Reads and mutations use FatFs; mount/unlock safety audits remain independent.
 * All mutations are inside VFS undo.
 * FatFs volume/cache is discarded for EVERY operation, including rollback.
 */
#include "fs_backend.h"
#include "../vendor/fatfs/ff.h"
enum { MPFF_FAT32=FS_FAT32, MPFF_EXFAT=FS_EXFAT };
#undef FS_FAT32
#undef FS_EXFAT
#define disk_read mpff_disk_read
#define disk_write mpff_disk_write
#define disk_initialize mpff_disk_initialize
#define disk_status mpff_disk_status
#define disk_ioctl mpff_disk_ioctl
#include "../vendor/fatfs/diskio.h"
#undef disk_read
#undef disk_write
static FATFS ffvol;
static int io_error;
/* Preserve the first bridge failure for the entire disposable FatFs view.
 * Cleanup must not issue new I/O or turn a failed mutation into a success. */
static void latch_error(int r){if(r<0&&io_error>=0)io_error=r;}
extern DWORD mpff_builtin_upcase(DWORD);
extern int multi_exfat_upcase(u16,u16*);
/* exFAT names MUST use this volume's $UpCase, not an unrelated Unicode table. */
DWORD ff_wtoupper(DWORD c){
 if(io_error<0)return c;
 if(vol.kind==FS_EXFAT&&c<65536){u16 v;int r=multi_exfat_upcase((u16)c,&v);if(r<0){latch_error(r);return c;}return v;}
 return mpff_builtin_upcase(c);
}
DSTATUS mpff_disk_initialize(BYTE p){return p||!vol.mounted?STA_NOINIT:0;}
DSTATUS mpff_disk_status(BYTE p){return mpff_disk_initialize(p)|(disk_writable()?0:STA_PROTECT);}
DRESULT mpff_disk_read(BYTE p,BYTE*b,LBA_t l,UINT n){
 if(io_error<0)return RES_ERROR;
 if(p||!b||!n||l>=vol.sectors||n>vol.sectors-l)return RES_PARERR;
 for(UINT i=0;i<n;i++){int r=disk_read(vol.base+l+i,b+512u*i);if(r<0){latch_error(r);return RES_ERROR;}}return RES_OK;
}
DRESULT mpff_disk_write(BYTE p,const BYTE*b,LBA_t l,UINT n){
 if(io_error<0)return RES_ERROR;
 if(p||!b||!n||l>=vol.sectors||n>vol.sectors-l)return RES_PARERR;
 if(!vol.writable)return RES_WRPRT;
 for(UINT i=0;i<n;i++){int r=disk_write(vol.base+l+i,b+512u*i);if(r<0){latch_error(r);return RES_ERROR;}}return RES_OK;
}
DRESULT mpff_disk_ioctl(BYTE p,BYTE c,void*b){
 if(io_error<0)return RES_ERROR;
 if(p)return RES_PARERR;
 if(c==CTRL_SYNC){int r=disk_flush();if(r<0){latch_error(r);return RES_ERROR;}return RES_OK;}
 if(!b)return RES_PARERR;
 if(c==GET_SECTOR_COUNT){*(LBA_t*)b=vol.sectors;return RES_OK;}
 if(c==GET_SECTOR_SIZE){*(WORD*)b=512;return RES_OK;}
 if(c==GET_BLOCK_SIZE){*(DWORD*)b=1;return RES_OK;}
 return RES_PARERR;
}
static int error(FRESULT r){if(io_error<0)return io_error;
 switch(r){case FR_OK:return 0;case FR_NO_FILE:case FR_NO_PATH:return NOTFOUND;
 case FR_EXIST:return EXISTS;case FR_WRITE_PROTECTED:return READONLY;
 case FR_DENIED:return NOSPACE;case FR_INVALID_NAME:case FR_INVALID_PARAMETER:return INVALID;
 case FR_NO_FILESYSTEM:return UNSUP;case FR_DISK_ERR:case FR_NOT_READY:return IO;default:return BAD;}
}
/* Upstream FatFs mounts FAT32 using FAT0 and mirrors to FAT1. Respect BPB
 * ExtFlags without modifying upstream: adjust the disposable FATFS only AFTER
 * its data/FSInfo geometry was computed from all physical FAT slots. */
static int select_fat(void){
 if(vol.kind!=FS_FAT32)return 0;
 u32 start,length,copies;int r=fat32_fat_layout(&start,&length,&copies);if(r<0)return r;
 if(ffvol.volbase || ffvol.fatbase!=vol.fat || ffvol.fsize!=length || !length ||
    ffvol.database!=vol.heap || ffvol.dirbase!=vol.root || ffvol.csize!=vol.spc ||
    ffvol.n_fatent!=vol.clusters+2 || start<ffvol.fatbase ||
    (start-ffvol.fatbase)%length || (start-ffvol.fatbase)/length>=ffvol.n_fats ||
    !copies || copies>ffvol.n_fats ||
    ((copies>1)&&(start!=ffvol.fatbase)))return BAD;
 ffvol.fatbase=start;ffvol.n_fats=(BYTE)copies;
 return 0;
}
static int begin(void){
 if(!vol.mounted||!vol.writable||vol.poisoned)return READONLY;
 f_mount(0,"",0);mzero(&ffvol,sizeof ffvol);io_error=0;
 int r=error(f_mount(&ffvol,"",1));
 if(r==0&&((vol.kind==FS_FAT32&&ffvol.fs_type!=MPFF_FAT32)||(vol.kind==FS_EXFAT&&ffvol.fs_type!=MPFF_EXFAT)))r=BAD;
 if(!r)r=select_fat();
 return r;
}
static int finish(int result){
 f_mount(0,"",0);
 if(io_error<0)return io_error;
 if(result<0)return result;
 u32 base=vol.base,n=vol.sectors;int kind=vol.kind;
 int r=kind==FS_EXFAT?exfat_mount_at(base,n):fat32_mount_at(base,n);
 if(r<0)return r;
 vol.kind=kind;vol.mounted=1;vol.writable=1;return 0;
}
static int writable_file(const char*path,FILINFO*info,int allow_missing){
 FRESULT r=f_stat(path,info);if(r==FR_NO_FILE&&allow_missing)return NOTFOUND;
 int e=error(r);if(e<0)return e;
 if(info->fattrib&AM_DIR)return ISDIR;
 if(info->fattrib&(AM_RDO|AM_SYS))return READONLY;
 return 0;
}
/* FatFs f_sync sets exFAT ValidDataLength = FileSize. Materialize ALL
 * uninitialized bytes before modifying an existing file; otherwise stale disk
 * contents become visible. A failed bounded transaction rolls this back. */
static int zeros(FIL *f,uint64_t from,uint64_t end){
 if(from>=end)return 0;u8 zero[512];mzero(zero,sizeof zero);
 int r=error(f_lseek(f,(FSIZE_t)from));if(r<0)return r;
 if((uint64_t)f_tell(f)!=from)return NOSPACE;
 while(from<end){UINT written,n=end-from>sizeof zero?sizeof zero:(UINT)(end-from);
  r=error(f_write(f,zero,n,&written));if(r<0)return r;if(written!=n)return NOSPACE;from+=n;
 }return 0;
}
int multi_fatfs_put(const char*path,const u8*data,u32 n,int mode,const char*source){
 MultiFile64 prior={0},source_view={0};
 if(mode==2&&vol.kind==FS_EXFAT){int r=fs_open64(path,&prior);if(r<0&&r!=NOTFOUND)return r;}
 if(mode==4){int r=fs_open64(source,&source_view);if(r<0)return r;}
 int e=begin();if(e<0){f_mount(0,"",0);return e;}
 if(mode==3)return finish(error(f_mkdir(path)));
 FILINFO info;int exists=writable_file(path,&info,1);
 if(exists!=0&&exists!=NOTFOUND)return finish(exists);
 if((mode==0||mode==4)&&exists==0)return finish(EXISTS);
 FIL dst,src;int opened=0;FRESULT r;
 if(mode==4){/* Source is read-only: RDO/SYSTEM protect mutations, not copying.
  * Its DATA was resolved through the guarded VFS view above. */
  r=f_stat(source,&info);e=error(r);if(e<0)return finish(e);
  if(info.fattrib&AM_DIR)return finish(ISDIR);
  r=f_open(&src,source,FA_READ);if(r!=FR_OK)return finish(error(r));opened=1;}
 BYTE flags=FA_WRITE|((mode==0||mode==4)?FA_CREATE_NEW:mode==1?FA_CREATE_ALWAYS:FA_OPEN_ALWAYS);
 r=f_open(&dst,path,flags);if(r!=FR_OK){e=error(r);if(opened)f_close(&src);return finish(e);}
 if(mode==2&&vol.kind==FS_EXFAT)e=zeros(&dst,prior.valid,prior.size);
 if(e>=0&&mode==2)e=error(f_lseek(&dst,f_size(&dst)));
 if(e>=0&&mode==4){u8 buf[512];UINT got,written;
  for(;;){r=f_read(&src,buf,sizeof buf,&got);if(r!=FR_OK){e=error(r);break;}if(!got)break;
   uint64_t start=(uint64_t)f_tell(&src)-got;
   u32 real=start>=source_view.valid?0:source_view.valid-start<got?(u32)(source_view.valid-start):got;
   if(real<got)mzero(buf+real,got-real);
   r=f_write(&dst,buf,got,&written);if(r!=FR_OK||written!=got){e=r==FR_OK?NOSPACE:error(r);break;}}
 }else if(e>=0){UINT written;r=f_write(&dst,data,n,&written);e=r==FR_OK?(written==n?0:NOSPACE):error(r);}
 r=f_close(&dst);if(e>=0)e=error(r);
 if(opened){r=f_close(&src);if(e>=0)e=error(r);}
 return finish(e);
}
int multi_fatfs_write_at(const char*path,uint64_t off,const u8*data,u32 n){
 MultiFile64 view;TRY(fs_open64(path,&view));
 if(off>view.size||(uint64_t)n>view.size-off)return INVALID;
 int e=begin();if(e<0){f_mount(0,"",0);return e;}
 FILINFO info;e=writable_file(path,&info,0);if(e<0)return finish(e);
 FIL f;FRESULT r=f_open(&f,path,FA_WRITE);if(r!=FR_OK)return finish(error(r));
 if(vol.kind==FS_EXFAT)e=zeros(&f,view.valid,view.size);
 if(e>=0)e=error(f_lseek(&f,(FSIZE_t)off));
 if(e>=0&&(uint64_t)f_tell(&f)!=off)e=NOSPACE;
 if(e>=0){UINT written;r=f_write(&f,data,n,&written);e=r==FR_OK?(written==n?0:NOSPACE):error(r);}
 r=f_close(&f);if(e>=0)e=error(r);return finish(e);
}
int multi_fatfs_resize(const char*path,uint64_t size){
 if(vol.kind==FS_FAT32&&size>UINT32_MAX)return UNSUP;
 MultiFile64 view;TRY(fs_open64(path,&view));
 int e=begin();if(e<0){f_mount(0,"",0);return e;}
 FILINFO info;e=writable_file(path,&info,0);if(e<0)return finish(e);
 FIL f;FRESULT r=f_open(&f,path,FA_WRITE);if(r!=FR_OK)return finish(error(r));
 uint64_t initialized=vol.kind==FS_EXFAT?view.valid:view.size;
 if(initialized<size)e=zeros(&f,initialized,size);
 if(e>=0)e=error(f_lseek(&f,(FSIZE_t)size));
 if(e>=0&&(uint64_t)f_tell(&f)!=size)e=NOSPACE;
 if(e>=0)e=error(f_truncate(&f));
 r=f_close(&f);if(e>=0)e=error(r);return finish(e);
}
int multi_fatfs_remove(const char*path,int dir){
 int e=begin();if(e<0){f_mount(0,"",0);return e;}
 FILINFO info;FRESULT r=f_stat(path,&info);if(r!=FR_OK)return finish(error(r));
 if((info.fattrib&AM_DIR)&&!dir)return finish(ISDIR);
 if(!(info.fattrib&AM_DIR)&&dir)return finish(NOTDIR);
 if(info.fattrib&(AM_RDO|AM_SYS))return finish(READONLY);
 r=f_unlink(path);return finish(r==FR_DENIED&&dir?NOTEMPTY:error(r));
}
int multi_fatfs_rename(const char*path,const char*leaf){
 if(!leaf||!*leaf)return INVALID;
 for(u32 i=0;leaf[i];i++)if(leaf[i]=='/'||leaf[i]=='\\'||leaf[i]==':')return INVALID;
 if(streq(leaf,".")||streq(leaf,".."))return INVALID;
 char target[PATHMAX];u32 slash=0,len=slen(path),n=slen(leaf);
 for(u32 i=0;i<len;i++)if(path[i]=='/')slash=i+1;
 if(slash+n>=PATHMAX)return TOOLONG;
 mcopy(target,path,slash);mcopy(target+slash,leaf,n+1);
 int e=begin();if(e<0){f_mount(0,"",0);return e;}
 FILINFO info;FRESULT r=f_stat(path,&info);if(r!=FR_OK)return finish(error(r));
 if(info.fattrib&(AM_RDO|AM_SYS))return finish(READONLY);
 return finish(error(f_rename(path,target)));
}

/* Read-only calls never enable writes or flush modified file objects. */
extern FRESULT mpff_object_next(FATFS*,Stream,u32*,Entry*);
extern FRESULT mpff_object_read(FATFS*,const MultiFile64*,uint64_t,u8*,u32);
static int begin_read(void){
 if(!vol.mounted||vol.poisoned)return BAD;
 f_mount(0,"",0);mzero(&ffvol,sizeof ffvol);io_error=0;
 int r=error(f_mount(&ffvol,"",1));
 if(!r&&((vol.kind==FS_FAT32&&ffvol.fs_type!=MPFF_FAT32)||(vol.kind==FS_EXFAT&&ffvol.fs_type!=MPFF_EXFAT)))r=BAD;
 if(!r)r=select_fat();
 return r;
}
int multi_fatfs_next(Stream d,u32*c,Entry*e){
 if(!c||!e)return INVALID;int r=begin_read();if(r<0){f_mount(0,"",0);return r;}
 FRESULT fr=mpff_object_next(&ffvol,d,c,e);r=fr==FR_NO_FILE?0:fr==FR_OK?1:error(fr);
 if(io_error<0)r=io_error;f_mount(0,"",0);return r;
}
int multi_fatfs_read64(const MultiFile64*f,uint64_t off,u8*b,u32 n){
 if(!f||(n&&!b)||f->valid>f->size||off>f->size||(uint64_t)n>f->size-off)return INVALID;
 if(!n)return 0;u32 initialized=off>=f->valid?0:(uint64_t)n>f->valid-off?(u32)(f->valid-off):n;
 int r=begin_read();if(r<0){f_mount(0,"",0);return r;}
 if(initialized)r=error(mpff_object_read(&ffvol,f,off,b,initialized));
 if(!r&&n>initialized)mzero(b+initialized,n-initialized);
 if(io_error<0)r=io_error;f_mount(0,"",0);return r;
}
int multi_fatfs_read(Stream s,u32 off,u8*b,u32 n){
 if(s.flags&MULTI_STREAM_WIDE)return UNSUP;MultiFile64 f={.data=s,.size=s.size,.valid=s.valid};return multi_fatfs_read64(&f,off,b,n);
}

int multi_fatfs_free(u32*count){
 if(!count)return INVALID;int r=begin_read();if(r<0){f_mount(0,"",0);return r;}
 FATFS*fs=0;DWORD freeclusters=0;r=error(f_getfree("",&freeclusters,&fs));
 if(!r&&(!fs||freeclusters>fs->n_fatent-2))r=BAD;
 if(!r)*count=freeclusters;if(io_error<0)r=io_error;f_mount(0,"",0);return r;
}
