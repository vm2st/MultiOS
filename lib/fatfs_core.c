/* vm2 studios | t.me/v2vproj — isolated freestanding FatFs compilation. */
#include "multi.h"
#define disk_read mpff_disk_read
#define disk_write mpff_disk_write
#define disk_initialize mpff_disk_initialize
#define disk_status mpff_disk_status
#define disk_ioctl mpff_disk_ioctl
#define memcpy mpff_memcpy
#define memset mpff_memset
#define memcmp mpff_memcmp
#define strlen mpff_strlen
#define strchr mpff_strchr
#include "../vendor/fatfs/ff.c"
void *mpff_memcpy(void*d,const void*s,size_t n){unsigned char*a=d;const unsigned char*b=s;while(n--)*a++=*b++;return d;}
void *mpff_memset(void*d,int c,size_t n){unsigned char*a=d;while(n--)*a++=(unsigned char)c;return d;}
int mpff_memcmp(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;while(n--){if(*x!=*y)return *x-*y;x++;y++;}return 0;}
size_t mpff_strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
char *mpff_strchr(const char*s,int c){for(;;s++){if(*s==(char)c)return (char*)s;if(!*s)return 0;}}

/* MultiPulti read-only object bridge. This stays next to the unmodified
 * upstream translation unit so no private FatFs functions are exported. */
FRESULT mpff_object_next(FATFS*fs,Stream s,u32*cursor,Entry*e){
 DIR d;FILINFO fi;FRESULT r;DEF_NAMEBUFF
 if(*cursor==UINT32_MAX)return FR_NO_FILE;
 mpff_memset(&d,0,sizeof d);d.obj.fs=fs;d.obj.id=fs->id;
 d.obj.sclust=s.root?0:s.first;d.obj.stat=s.flags&2;d.obj.objsize=s.size;
 INIT_NAMEBUFF(fs);
 r=dir_sdi(&d,*cursor);
 if(r==FR_OK)r=DIR_READ_FILE(&d);
 if(r==FR_OK){
  get_fileinfo(&d,&fi);mpff_memset(e,0,sizeof *e);
  size_t len=mpff_strlen(fi.fname);e->name_valid=len<sizeof e->name;
  if(e->name_valid)mpff_memcpy(e->name,fi.fname,len+1);else mpff_memcpy(e->name,"[name exceeds UTF-8 path limit]",31);
  len=mpff_strlen(fi.altname);if(len<sizeof e->alias)mpff_memcpy(e->alias,fi.altname,len+1);
  e->ascii=1;for(size_t i=0;e->name[i];i++)if((unsigned char)e->name[i]>=128)e->ascii=0;
  e->attr=fi.fattrib;e->wide=1;
  if(fs->fs_type==FS_EXFAT){e->data.first=ld_32(fs->dirbuf+XDIR_FstClus);e->data.flags=fs->dirbuf[XDIR_GenFlags]&2;
   e->size64=ld_64(fs->dirbuf+XDIR_FileSize);e->valid64=ld_64(fs->dirbuf+XDIR_ValidFileSize);}
  else{e->data.first=ld_clust(fs,d.dir);e->size64=e->valid64=fi.fsize;}
  if(e->valid64>e->size64)r=FR_INT_ERR;
  e->data.size=e->size64>UINT32_MAX?UINT32_MAX:(u32)e->size64;e->data.valid=e->valid64>UINT32_MAX?UINT32_MAX:(u32)e->valid64;
  if(e->size64>UINT32_MAX){e->large=MULTI_ENTRY_SIZE64;e->data.flags|=MULTI_STREAM_WIDE;}
  if(r==FR_OK){r=dir_next(&d,0);if(r==FR_NO_FILE){*cursor=UINT32_MAX;r=FR_OK;}else if(r==FR_OK)*cursor=d.dptr;}
 }
 FREE_NAMEBUFF();return r;
}
FRESULT mpff_object_read(FATFS*fs,const MultiFile64*view,uint64_t off,u8*b,u32 n){
 FIL f;UINT got;mpff_memset(&f,0,sizeof f);f.obj.fs=fs;f.obj.id=fs->id;
 f.obj.sclust=view->data.first;f.obj.objsize=view->size;f.obj.stat=view->data.flags&2;f.flag=FA_READ;
 FRESULT r=f_lseek(&f,off);if(r!=FR_OK)return r;if(f.fptr!=off)return FR_INT_ERR;
 r=f_read(&f,b,n,&got);return r==FR_OK&&got!=n?FR_INT_ERR:r;
}
