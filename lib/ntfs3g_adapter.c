/* SPDX-License-Identifier: GPL-2.0-or-later
 * MultiPulti NTFS-3G bridge | vm2 studios | t.me/v2vproj
 * Experimental opt-in: regular files/directories, clean NTFS 3.x, protected
 * image only. No FUSE or host OS services in firmware. Not full NTFS support.
 */
#include "fs_backend.h"
#include "unicode.h"
#include "multios/read_cache.h"
#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "../vendor/ntfs-3g/include/ntfs-3g/volume.h"
#include "../vendor/ntfs-3g/include/ntfs-3g/device.h"
#include "../vendor/ntfs-3g/include/ntfs-3g/attrib.h"
#include "../vendor/ntfs-3g/include/ntfs-3g/inode.h"
#include "../vendor/ntfs-3g/include/ntfs-3g/dir.h"
#include "../vendor/ntfs-3g/include/ntfs-3g/endians.h"
extern int mpntfs_check_clean(ntfs_volume*);
extern void mpntfs_reset(void);
static s64 position;
static int io_error;
#ifndef MULTIOS_NTFS_READ_CACHE
#define MULTIOS_NTFS_READ_CACHE 1
#endif
#if MULTIOS_NTFS_READ_CACHE
static MultiReadCache ro_cache;
static int cache_enabled;
static int cache_sector(void *ctx,u32 lba,u8 *data){(void)ctx;return disk_read(lba,data);}
static void clear_cache(void){multi_read_cache_clear(&ro_cache);}
static int read_sector(u32 lba,u8 *data){
 if(vol.poisoned){clear_cache();return IO;}
 return cache_enabled?multi_read_cache_get(&ro_cache,0,cache_sector,lba,data):disk_read(lba,data);
}
#else
static void clear_cache(void){}
static int read_sector(u32 lba,u8 *data){return disk_read(lba,data);}
#endif
static int maperr(void){if(io_error<0)return io_error;switch(errno){
 case ENOENT:return NOTFOUND;case EEXIST:return EXISTS;case ENOSPC:case ENOMEM:return NOSPACE;
 case EROFS:case EACCES:case EPERM:return READONLY;case ENOTDIR:return NOTDIR;case EISDIR:return ISDIR;
 case ENOTEMPTY:return NOTEMPTY;case EINVAL:return INVALID;case EOPNOTSUPP:return UNSUP;default:return IO;}}
static int devopen(struct ntfs_device*d,int flags){
 if(NDevOpen(d)){errno=EBUSY;return -1;}
 if((flags&O_ACCMODE)!=O_RDONLY&&!vol.writable){errno=EROFS;return -1;}
 NDevSetOpen(d);if((flags&O_ACCMODE)==O_RDONLY)NDevSetReadOnly(d);else NDevClearReadOnly(d);
 position=0;return 0;
}
static int devsync(struct ntfs_device*d){(void)d;if(io_error<0){errno=EIO;return -1;}int r=disk_flush();if(r<0){if(io_error>=0)io_error=r;errno=EIO;return -1;}return 0;}
static int devclose(struct ntfs_device*d){NDevClearOpen(d);return devsync(d);}
static s64 devseek(struct ntfs_device*d,s64 offset,int whence){(void)d;
 s64 size=(s64)vol.sectors*512,base=whence==0?0:whence==1?position:whence==2?size:-1;
 if(base<0||offset < -base||offset>size-base){errno=EINVAL;return -1;}position=base+offset;return position;}
static s64 transfer(void*buf,s64 n,s64 off,int write){
 if(io_error<0){errno=EIO;return -1;}
 s64 size=(s64)vol.sectors*512;if(n<0||off<0||off>size||n>size-off){errno=EINVAL;return -1;}
 if(write&&!vol.writable){errno=EROFS;return -1;}
 u8 sector[512],*p=buf;s64 done=0;
 while(done<n){u32 l=(u32)((off+done)/512),skip=(u32)((off+done)%512),take=512-skip;
  if((s64)take>n-done)take=(u32)(n-done);int r;
  if(!write||skip||take<512){r=read_sector(vol.base+l,sector);if(r<0){if(io_error>=0)io_error=r;errno=EIO;return -1;}}
  if(write){mcopy(sector+skip,p+done,take);r=disk_write(vol.base+l,sector);if(r<0){if(io_error>=0)io_error=r;errno=EIO;return -1;}}
  else mcopy(p+done,sector+skip,take);done+=take;
 }return done;
}
static s64 devpread(struct ntfs_device*d,void*b,s64 n,s64 o){(void)d;return transfer(b,n,o,0);}
static s64 devpwrite(struct ntfs_device*d,const void*b,s64 n,s64 o){if(NDevReadOnly(d)){errno=EROFS;return -1;}return transfer((void*)b,n,o,1);}
static s64 devread(struct ntfs_device*d,void*b,s64 n){s64 r=devpread(d,b,n,position);if(r>0)position+=r;return r;}
static s64 devwrite(struct ntfs_device*d,const void*b,s64 n){s64 r=devpwrite(d,b,n,position);if(r>0)position+=r;return r;}
static int devstat(struct ntfs_device*d,struct stat*s){(void)d;mzero(s,sizeof *s);s->st_mode=S_IFREG|0600;s->st_size=(s64)vol.sectors*512;s->st_blksize=512;return 0;}
static int devioctl(struct ntfs_device*d,unsigned long request,void*p){(void)d;(void)request;(void)p;errno=ENOTTY;return -1;}
static struct ntfs_device_operations operations={devopen,devclose,devseek,devread,devwrite,devpread,devpwrite,devsync,devstat,devioctl};
static ntfs_volume*open_volume(int ro){
 mpntfs_reset();io_error=0;
#if MULTIOS_NTFS_READ_CACHE
 cache_enabled=ro;if(!ro)clear_cache();
#endif
 struct ntfs_device*d=ntfs_device_alloc("MultiPulti-HAL",0,&operations,0);if(!d)return 0;
 ntfs_volume*v=ntfs_device_mount(d,ro?NTFS_MNT_RDONLY:NTFS_MNT_NONE);
 if(!v){int saved=errno;ntfs_device_free(d);errno=saved;}
 else if(ntfs_set_ignore_case(v)){int saved=errno;ntfs_umount(v,0);v=0;errno=saved;}
 if(v){NVolSetCaseSensitive(v);NVolSetShowHidFiles(v);NVolSetShowSysFiles(v);}return v;
}
int multi_ntfs3g_mount(u32 base,u32 limit){return multi_ntfs3g_mount_full(base,limit);}
int multi_ntfs3g_unlock(void){
 vol.writable=0;if(!vol.mounted||vol.poisoned)return BAD;if(!disk_writable())return READONLY;
 if(vol.flags&2)return BAD;
 ntfs_volume*v=open_volume(1);if(!v){int r=maperr();mpntfs_reset();return r;}
 int r=0;if(v->flags&VOLUME_IS_DIRTY)r=BAD;
 if(!r&&(ntfs_version_is_supported(v)||ntfs_volume_check_hiberfile(v,1)||mpntfs_check_clean(v)))r=maperr();
 if(ntfs_umount(v,0)&&!r)r=maperr();mpntfs_reset();if(!r)vol.writable=1;return r;
}
static int finish(ntfs_volume*v,int r){
 clear_cache();
 /* An operation failure is recovered by sector undo, not by flushing stale
  * NTFS caches. All allocations belong to this disposable volume scope. */
 if(r<0||io_error<0){if(io_error<0)r=io_error;mpntfs_reset();return r;}
 int closed=ntfs_umount(v,0);
 /* Upstream may report an aggregate close success after a deferred write
  * failed. Preserve HAL/undo errors BEFORE reset/remount clears io_error. */
 if(io_error<0)r=io_error;else if(closed&&r>=0)r=maperr();
 mpntfs_reset();if(r<0)return r;
 u32 base=vol.base,n=vol.sectors;r=multi_ntfs3g_mount(base,n);if(r<0)return r;
 vol.mounted=1;vol.writable=1;vol.kind=FS_NTFS;return 0;
}
static int safe_inode(ntfs_inode*i,int directory){
 if(i->mft_no<FILE_first_user)return READONLY;
 if(i->flags&(FILE_ATTR_READONLY|FILE_ATTR_SYSTEM))return READONLY;
 if(i->flags&(FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED))return UNSUP;
 int isdir=(i->mrec->flags&MFT_RECORD_IS_DIRECTORY)!=0;
 return directory?(isdir?0:NOTDIR):(isdir?ISDIR:0);
}
/* A copy source is never modified. Do not require mutation permission just
 * to read ordinary DATA, but retain metadata/reparse/encryption exclusions. */
static int safe_copy_source(ntfs_inode*i){
 if(i->mft_no<FILE_first_user)return READONLY;
 if(i->mrec->flags&MFT_RECORD_IS_DIRECTORY)return ISDIR;
 if(i->flags&(FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED))return UNSUP;
 return 0;
}
/* Do not use upstream ignore-case pathname lookup: its collation may select
 * a folded sibling before the exact POSIX name. Enumerate using stored names.
 * This stays inside one live volume scope, never calls the resetting VFS. */
struct NameChoice {ntfs_volume*v;const ntfschar*want;u32 units,exact,fold,steps;u64 er,fr;ntfschar en[255],fn[255];u32 el,fl;int error;};
static int choose_name(void*opaque,const ntfschar*name,const int units,const int type,const s64 pos,const MFT_REF ref,const unsigned dt){
 (void)pos;(void)dt;struct NameChoice*c=opaque;
 if(++c->steps>1048576){c->error=EOPNOTSUPP;return 1;}
 if(type==FILE_NAME_DOS)return 0;
 if(units<1||units>255){c->error=EINVAL;return 1;}
 if(ntfs_names_are_equal(c->want,c->units,name,units,IGNORE_CASE,c->v->upcase,c->v->upcase_len)){
  if(c->fold<2)c->fold++;c->fr=ref;c->fl=units;mcopy(c->fn,name,units*2);
  if(ntfs_names_are_equal(c->want,c->units,name,units,CASE_SENSITIVE,c->v->upcase,c->v->upcase_len)){
   if(c->exact<2)c->exact++;c->er=ref;c->el=units;mcopy(c->en,name,units*2);
  }
 }
 return 0;
}
static int select_name(ntfs_inode*parent,const ntfschar*want,u32 units,u64*ref,ntfschar*stored,u8*length){
 struct NameChoice c={.v=parent->vol,.want=want,.units=units};s64 pos=0;
 if(ntfs_readdir(parent,&pos,&c,choose_name)||c.error){if(c.error)errno=c.error;return -1;}
 if(c.exact>1){errno=EIO;return -1;}
 if(!c.exact&&c.fold>1){errno=EOPNOTSUPP;return -1;}
 if(!c.exact&&!c.fold){errno=ENOENT;return -1;}
 *ref=c.exact?c.er:c.fr;u32 n=c.exact?c.el:c.fl;
 if(stored)mcopy(stored,c.exact?c.en:c.fn,n*2);if(length)*length=(u8)n;return 0;
}
static ntfs_inode*path_inode(ntfs_volume*v,const char*path){
 if(!path||path[0]!='/'||slen(path)>=PATHMAX){errno=EINVAL;return 0;}
 ntfs_inode*ni=ntfs_inode_open(v,FILE_root);if(!ni)return 0;
 const char*p=path+1;
 while(*p){
  char part[PATHMAX];u32 n=0,units;ntfschar name[255];u64 ref;
  while(*p&&*p!='/')part[n++]=*p++;part[n]=0;
  if(!n||multi_utf8_to16(part,(u16*)name,255,&units)<0){errno=EINVAL;goto fail;}
  if(!(ni->mrec->flags&MFT_RECORD_IS_DIRECTORY)){errno=ENOTDIR;goto fail;}
  if(select_name(ni,name,units,&ref,0,0))goto fail;
  if(ntfs_inode_close(ni))return 0;ni=ntfs_inode_open(v,ref);if(!ni)return 0;
  if(*p){p++;if(!*p){errno=EINVAL;goto fail;}}
 }
 return ni;
 fail:{int saved=errno;ntfs_inode_close(ni);errno=saved;return 0;}
}
static int parent_name(ntfs_volume*v,const char*path,ntfs_inode**parent,ntfschar*name,u8*units){
 u32 len=slen(path),slash=0,n;char dir[PATHMAX];if(len>=PATHMAX||!len)return INVALID;
 for(u32 i=0;i<len;i++)if(path[i]=='/')slash=i+1;
 if(!slash||!path[slash]||multi_name_valid(path+slash)<0)return INVALID;
 int r=multi_utf8_to16(path+slash,(u16*)name,255,&n);if(r<0)return r;if(!n||n>255)return TOOLONG;
 *units=(u8)n;if(slash==1){dir[0]='/';dir[1]=0;}else{mcopy(dir,path,slash-1);dir[slash-1]=0;}
 *parent=path_inode(v,dir);if(!*parent)return maperr();
 if(!((*parent)->mrec->flags&MFT_RECORD_IS_DIRECTORY)){ntfs_inode_close(*parent);*parent=0;return NOTDIR;}
 if((*parent)->mft_no!=FILE_root&&((*parent)->mft_no<FILE_first_user||((*parent)->flags&FILE_ATTR_SYSTEM))){ntfs_inode_close(*parent);*parent=0;return READONLY;}
 if((*parent)->flags&(FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED)){ntfs_inode_close(*parent);*parent=0;return UNSUP;}
 u64 ref;ntfschar stored[255];u8 length;
 if(!select_name(*parent,name,*units,&ref,stored,&length)){mcopy(name,stored,length*2);*units=length;}
 else if(errno!=ENOENT)return maperr();
 return 0;
}
int multi_ntfs3g_put(const char*path,const u8*b,u32 n,int mode,const char*source){
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 int r=0;ntfs_inode*parent=0,*ni=0,*src=0;ntfs_attr*na=0,*sa=0;ntfschar name[256];u8 units;
 r=parent_name(v,path,&parent,name,&units);if(r<0)goto end;
 ni=path_inode(v,path);
 if(ni&&(mode==0||mode==3||mode==4)){r=EXISTS;goto end;}
 if(!ni&&errno!=ENOENT){r=maperr();goto end;}
 if(mode==4){src=path_inode(v,source);if(!src){r=maperr();goto end;}
  r=safe_copy_source(src);if(r<0)goto end;sa=ntfs_attr_open(src,AT_DATA,AT_UNNAMED,0);if(!sa){r=maperr();goto end;}}
 if(!ni){ni=ntfs_create(parent,0,name,units,mode==3?S_IFDIR:S_IFREG);if(!ni){r=maperr();goto end;}}
 else {r=safe_inode(ni,0);if(r<0)goto end;}
 if(mode==3)goto end;
 na=ntfs_attr_open(ni,AT_DATA,AT_UNNAMED,0);if(!na){r=maperr();goto end;}
 if(mode==1&&ntfs_attr_truncate(na,0)){r=maperr();goto end;}
 s64 off=mode==2?na->data_size:0;
 if(mode==4){u8 buf[512];s64 pos=0;while(pos<sa->data_size){s64 count=sa->data_size-pos;if(count>512)count=512;
  if(ntfs_attr_pread(sa,pos,count,buf)!=count||ntfs_attr_pwrite(na,pos,count,buf)!=count){r=maperr();goto end;}pos+=count;}}
 else if(n&&ntfs_attr_pwrite(na,off,n,b)!=n){r=maperr();goto end;}
 ntfs_inode_update_times(ni,NTFS_UPDATE_MCTIME);
end:
 if(r<0||io_error<0)return finish(v,r);
 if(na){if(ntfs_attr_pclose(na))return finish(v,maperr());ntfs_attr_close(na);}if(sa)ntfs_attr_close(sa);
 if(io_error<0)return finish(v,io_error);
 if(src&&ntfs_inode_close(src))return finish(v,maperr());
 /* Publish parent index before child filename sync reopens that parent.
  * Avoid close_in_dir: its failed-index path can consume the borrowed parent. */
 if(parent&&ntfs_inode_close(parent))return finish(v,maperr());
 if(ni&&ntfs_inode_close(ni))return finish(v,maperr());
 return finish(v,r);
}
/* Exact resize is shared by unnamed DATA and ADS. Compressed growth must
 * be driven by writes: upstream truncate may stop at a compression boundary. */
static int resize_attribute(ntfs_attr *na,uint64_t size){
 if(na->data_size<0)return BAD;
 if(size>(uint64_t)na->data_size&&(na->data_flags&ATTR_COMPRESSION_MASK)){
  u8 zero[8192];mzero(zero,sizeof zero);uint64_t pos=(uint64_t)na->data_size;
  while(pos<size){u32 n=size-pos>sizeof zero?sizeof zero:(u32)(size-pos);
   if(ntfs_attr_pwrite(na,(s64)pos,n,zero)!=n)return maperr();pos+=n;}
  return 0;
 }
 return ntfs_attr_truncate(na,(s64)size)?maperr():0;
}
static int random_mutation(const char*path,uint64_t off,const u8*data,u32 n,int resize){
 if(off>INT64_MAX||(uint64_t)n>INT64_MAX-off)return INVALID;
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 ntfs_inode*ni=path_inode(v,path);if(!ni)return finish(v,maperr());
 int r=safe_inode(ni,0);if(r<0)return finish(v,r);
 ntfs_attr*na=ntfs_attr_open(ni,AT_DATA,AT_UNNAMED,0);if(!na)return finish(v,maperr());
 if(na->data_size<0)return finish(v,BAD);
 if(resize)r=resize_attribute(na,off);
 else if(off>(uint64_t)na->data_size||(uint64_t)n>(uint64_t)na->data_size-off)r=INVALID;
 else if(n&&ntfs_attr_pwrite(na,(s64)off,n,data)!=n)r=maperr();
 if(r<0||io_error<0)return finish(v,r);
 if(ntfs_attr_pclose(na))return finish(v,maperr());
 if(resize&&(uint64_t)na->data_size!=off)return finish(v,BAD);
 ntfs_attr_close(na);
 ntfs_inode_update_times(ni,NTFS_UPDATE_MCTIME);
 if(ntfs_inode_close(ni))return finish(v,maperr());return finish(v,0);
}
int multi_ntfs3g_write_at(const char*p,uint64_t off,const u8*b,u32 n){return random_mutation(p,off,b,n,0);}
int multi_ntfs3g_resize(const char*p,uint64_t size){return random_mutation(p,size,0,0,1);}
int multi_ntfs3g_remove(const char*path,int dir){
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 ntfs_inode*parent=0,*ni=0;ntfschar name[256];u8 units;int r=parent_name(v,path,&parent,name,&units);
 if(r>=0){ni=path_inode(v,path);r=ni?safe_inode(ni,dir):maperr();}
 if(r>=0&&dir&&ntfs_check_empty_dir(ni))r=maperr();
 if(r>=0){r=ntfs_delete(v,0,ni,parent,name,units)?maperr():0;ni=parent=0;}
 if(r<0||io_error<0)return finish(v,r);
 if(ni&&ntfs_inode_close(ni))r=maperr();if(parent&&r>=0&&ntfs_inode_close(parent))r=maperr();return finish(v,r);
}
int multi_ntfs3g_rename(const char*path,const char*leaf){
 if(!leaf||multi_name_valid(leaf)<0)return INVALID;
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 ntfs_inode*parent=0,*ni=0;ntfschar old[256],name[256];u8 oldn;u32 n;
 int r=multi_utf8_to16(leaf,(u16*)name,255,&n);if(r<0||!n||n>255){r=INVALID;goto end;}
 r=parent_name(v,path,&parent,old,&oldn);if(r<0)goto end;
 ni=path_inode(v,path);if(!ni){r=maperr();goto end;}
 r=safe_inode(ni,(ni->mrec->flags&MFT_RECORD_IS_DIRECTORY)!=0);if(r<0)goto end;
 if(ntfs_link(ni,parent,name,(u8)n)){r=maperr();goto end;}
 r=ntfs_delete(v,0,ni,parent,old,oldn)?maperr():0;ni=parent=0;
end:
 if(r<0||io_error<0)return finish(v,r);
 if(ni&&ntfs_inode_close(ni))r=maperr();if(parent&&r>=0&&ntfs_inode_close(parent))r=maperr();return finish(v,r);
}

/* Read side uses the SAME engine as mutations; no bounded MFT scan or
 * MultiOS 32-run decoding path. Stateless inode IDs survive fresh RO mounts. */
static ntfschar read_upcase[65536];
static u32 read_upcase_len;
static int read_finish(int r){if(io_error<0)r=io_error;if(r<0)clear_cache();mpntfs_reset();return r;}
static int read_case(u16 c,u16*v){if(c>=read_upcase_len)return BAD;*v=le16_to_cpu(read_upcase[c]);return 0;}
int multi_ntfs3g_equal(const char*a,const char*b){return multi_name_equal(a,b,read_case);}
int multi_ntfs3g_mount_full(u32 base,u32 limit){
 clear_cache();read_upcase_len=0;if(!limit||base>=disk_sectors()||limit>disk_sectors()-base)return BAD;
 vol.base=base;vol.sectors=limit;vol.writable=0;u8 boot[512];TRY(disk_read(base,boot));
 if(boot[11]!=0||boot[12]!=2)return UNSUP; /* 512-byte NTFS sectors */
 ntfs_volume*v=open_volume(1);if(!v){int r=maperr();return read_finish(r);}
 int r=0;
 if(!v->upcase||v->upcase_len!=65536||v->nr_clusters<=0||v->nr_clusters>UINT32_MAX||!v->cluster_size||(v->cluster_size&511))r=UNSUP;
 if(!r){
  for(u32 i=0;i<128;i++){u16 want=i>='a'&&i<='z'?i-32:i;if(le16_to_cpu(v->upcase[i])!=want){r=BAD;break;}}
 }
 if(!r){mcopy(read_upcase,v->upcase,65536u*2);read_upcase_len=65536;
  vol.clusters=(u32)v->nr_clusters;vol.cluster_bytes=v->cluster_size;vol.spc=v->cluster_size/512;vol.root=FILE_root;
  vol.flags=(v->flags&VOLUME_IS_DIRTY)?2:0;vol.kind=FS_NTFS;}
 return read_finish(r);
}
static int read_inode(ntfs_inode*ni,const MultiFile64*f,uint64_t off,u8*b,u32 n){
 if(ni->mrec->flags&MFT_RECORD_IS_DIRECTORY)return ISDIR;
 if(ni->flags&FILE_ATTR_ENCRYPTED)return UNSUP;
 ntfs_attr*na=ntfs_attr_open(ni,AT_DATA,AT_UNNAMED,0);
 if(!na){if(errno==ENOENT&&!f->size&&!n)return 0;return maperr();}
 if(na->data_size<0||na->initialized_size<0||na->initialized_size>na->data_size||(uint64_t)na->data_size!=f->size)return BAD;
 if(n&&ntfs_attr_pread(na,(s64)off,n,b)!=(s64)n)return maperr();return 0;
}
int multi_ntfs3g_read64(const MultiFile64*f,uint64_t off,u8*b,u32 n){
 if(!f||(n&&!b)||f->valid>f->size||off>f->size||(uint64_t)n>f->size-off||off>INT64_MAX)return INVALID;
 if(!n)return 0;ntfs_volume*v=open_volume(1);if(!v)return read_finish(maperr());
 ntfs_inode*ni=ntfs_inode_open(v,f->data.first);if(!ni)return read_finish(maperr());
 return read_finish(read_inode(ni,f,off,b,n));
}
int multi_ntfs3g_read(Stream s,u32 off,u8*b,u32 n){
 if(s.flags&MULTI_STREAM_WIDE)return UNSUP;MultiFile64 f={.data=s,.size=s.size,.valid=s.valid};return multi_ntfs3g_read64(&f,off,b,n);
}
struct ReadEntry{Entry*entry;ntfs_volume*volume;int found,error;s64 next;};
static int read_entry(void*opaque,const ntfschar*name,int units,int type,s64 pos,MFT_REF ref,unsigned dt){
 (void)dt;struct ReadEntry*c=opaque;
 if(type==FILE_NAME_DOS||(units==1&&le16_to_cpu(name[0])=='.')||(units==2&&le16_to_cpu(name[0])=='.'&&le16_to_cpu(name[1])=='.'))return 0;
 if(pos<0||pos>=UINT32_MAX||MREF(ref)>UINT32_MAX){c->error=UNSUP;return 1;}
 Entry*e=c->entry;mzero(e,sizeof *e);
 u16 ucs[255];if(units<1||units>255){c->error=BAD;return 1;}
 for(int i=0;i<units;i++)ucs[i]=le16_to_cpu(name[i]);
 int r=multi_name_entry(ucs,(u32)units,e);if(r<0){c->error=r;return 1;}
 ntfs_inode*ni=ntfs_inode_open(c->volume,ref);if(!ni){c->error=maperr();return 1;}
 e->data.first=(u32)ni->mft_no;e->wide=1;
 if(ni->mrec->flags&MFT_RECORD_IS_DIRECTORY){e->attr|=ATTR_DIR;e->data.flags=ATTR_DIR;}
 else{ntfs_attr*na=ntfs_attr_open(ni,AT_DATA,AT_UNNAMED,0);
  if(na){if(na->data_size<0||na->initialized_size<0||na->initialized_size>na->data_size){c->error=BAD;return 1;}
   e->size64=(uint64_t)na->data_size;e->valid64=(uint64_t)na->initialized_size;ntfs_attr_close(na);}
  else if(errno!=ENOENT){c->error=maperr();return 1;}
  if(ni->flags&FILE_ATTR_ENCRYPTED)e->large|=MULTI_ENTRY_FEATURE;
 }
 if(ni->flags&FILE_ATTR_REPARSE_POINT)e->large|=MULTI_ENTRY_FEATURE;
 if(ni->flags&FILE_ATTR_READONLY)e->attr|=1;if(ni->flags&FILE_ATTR_HIDDEN)e->attr|=2;if(ni->flags&FILE_ATTR_SYSTEM)e->attr|=4;
 e->data.size=e->size64>UINT32_MAX?UINT32_MAX:(u32)e->size64;e->data.valid=e->valid64>UINT32_MAX?UINT32_MAX:(u32)e->valid64;
 if(e->size64>UINT32_MAX){e->large|=MULTI_ENTRY_SIZE64;e->data.flags|=MULTI_STREAM_WIDE;}
 if(ntfs_inode_close(ni)){c->error=maperr();return 1;}
 c->found=1;c->next=pos+1;return 1;
}
int multi_ntfs3g_next(Stream d,u32*cursor,Entry*e){
 if(!cursor||!e)return INVALID;if(*cursor==UINT32_MAX)return 0;
 ntfs_volume*v=open_volume(1);if(!v)return read_finish(maperr());
 ntfs_inode*ni=ntfs_inode_open(v,d.first);if(!ni)return read_finish(maperr());
 /* Ignore-case lookup is not ignore-case DISPLAY: ntfs_readdir otherwise
  * lowercases names. Preserve on-disk spelling for VFS directory entries.
  * This disposable read-only volume uses inode IDs, not pathname lookup. */
 NVolSetCaseSensitive(v);
 struct ReadEntry c={.entry=e,.volume=v};s64 pos=*cursor;int r=ntfs_readdir(ni,&pos,&c,read_entry);
 if(c.error)return read_finish(c.error);
 if(c.found){*cursor=(u32)c.next;return read_finish(1);}
 if(r)return read_finish(maperr());*cursor=UINT32_MAX;return read_finish(0);
}
int multi_ntfs3g_free(u32*count){
 if(!count)return INVALID;ntfs_volume*v=open_volume(1);if(!v)return read_finish(maperr());
 if(ntfs_volume_get_free_space(v))return read_finish(maperr());
 if(v->free_clusters<0||v->free_clusters>UINT32_MAX)return read_finish(BAD);*count=(u32)v->free_clusters;return read_finish(0);
}

/* NTFS named $DATA and hardlinks. No colon-based path rewriting and no access
 * to arbitrary NTFS metadata attributes. Every mutator is wrapped by VFS undo. */
static int stream_name16(const char *s,ntfschar *out,u32 *units){
 int r=multi_stream_name_valid(s);if(r<0)return r;
 return multi_utf8_to16(s,(u16*)out,255,units);
}
static int stream_inode_allowed(ntfs_inode *ni,int write){
 if(ni->mrec->flags&MFT_RECORD_IS_DIRECTORY)return ISDIR;
 if(ni->mft_no<FILE_first_user)return READONLY;
 if(ni->flags&(FILE_ATTR_REPARSE_POINT|FILE_ATTR_ENCRYPTED))return UNSUP;
 return write?safe_inode(ni,0):0;
}
/* Exact spelling first. A case-insensitive fallback must be unique. */
static ntfs_attr *open_named_data(ntfs_inode *ni,const ntfschar *requested,u32 units){
 ntfs_attr *na=ntfs_attr_open(ni,AT_DATA,(ntfschar*)requested,units);if(na||errno!=ENOENT)return na;
 ntfs_attr_search_ctx*ctx=ntfs_attr_get_search_ctx(ni,0);if(!ctx)return 0;
 ntfschar stored[255];u32 count=0,matches=0,steps=0;int saved=ENOENT;
 while(!ntfs_attrs_walk(ctx)){
  if(++steps>16384){saved=EOPNOTSUPP;goto done;}
  ATTR_RECORD*a=ctx->attr;
  if(a->type!=AT_DATA||!a->name_length||(a->non_resident&&sle64_to_cpu(a->lowest_vcn)))continue;
  u32 len=le32_to_cpu(a->length),off=le16_to_cpu(a->name_offset),n=a->name_length;
  if(off>len||n*2>len-off){saved=EINVAL;goto done;}
  ntfschar name[255];mcopy(name,(u8*)a+off,n*2);
  if(ntfs_names_are_equal(requested,units,name,n,IGNORE_CASE,ni->vol->upcase,ni->vol->upcase_len)){
   if(++matches>1){saved=EOPNOTSUPP;goto done;}
   count=n;mcopy(stored,name,n*2);
  }
 }
 if(errno!=ENOENT){saved=errno;goto done;}
 ntfs_attr_put_search_ctx(ctx);
 if(matches)return ntfs_attr_open(ni,AT_DATA,stored,count);
 errno=ENOENT;return 0;
 done:ntfs_attr_put_search_ctx(ctx);errno=saved;return 0;
}
int multi_ntfs3g_stream_next(const char *path,u32 *cursor,MultiNamedStream *info){
 if(*cursor==UINT32_MAX)return 0;if(*cursor>=4096)return UNSUP;
 ntfs_volume*v=open_volume(1);if(!v)return read_finish(maperr());
 ntfs_inode*ni=path_inode(v,path);if(!ni)return read_finish(maperr());
 int r=stream_inode_allowed(ni,0);if(r<0)return read_finish(r);
 ntfs_attr_search_ctx*ctx=ntfs_attr_get_search_ctx(ni,0);if(!ctx)return read_finish(maperr());
 u32 index=0,steps=0;
 while(!ntfs_attrs_walk(ctx)){
  if(++steps>16384)return read_finish(UNSUP);
  ATTR_RECORD*a=ctx->attr;if(a->type!=AT_DATA||!a->name_length)continue;
  if(a->non_resident&&sle64_to_cpu(a->lowest_vcn)!=0)continue;
  if(index++<*cursor)continue;
  u32 len=le32_to_cpu(a->length),off=le16_to_cpu(a->name_offset),units=a->name_length;
  if(off>len||units*2>len-off)return read_finish(BAD);
  ntfschar name[255];u16 native[255];
  for(u32 i=0;i<units;i++){mcopy(&name[i],(u8*)a+off+i*2,2);native[i]=le16_to_cpu(name[i]);}
  MultiNamedStream result; mzero(&result,sizeof result);
  r=multi_utf16_to8(native,units,result.name,sizeof result.name);if(r<0)return read_finish(r);
  if(multi_stream_name_valid(result.name)<0)return read_finish(UNSUP);
  ntfs_attr*na=ntfs_attr_open(ni,AT_DATA,name,units);if(!na)return read_finish(maperr());
  if(na->data_size<0||na->initialized_size<0||na->initialized_size>na->data_size)return read_finish(BAD);
  result.size=(uint64_t)na->data_size;result.valid=(uint64_t)na->initialized_size;
  result.flags=le16_to_cpu(na->data_flags);*info=result;*cursor=index;return read_finish(1);
 }
 if(errno!=ENOENT)return read_finish(maperr());*cursor=UINT32_MAX;return read_finish(0);
}
int multi_ntfs3g_stream_read(const char *path,const char *stream,uint64_t off,u8*b,u32 n){
 ntfschar name[255];u32 units;TRY(stream_name16(stream,name,&units));
 ntfs_volume*v=open_volume(1);if(!v)return read_finish(maperr());
 ntfs_inode*ni=path_inode(v,path);if(!ni)return read_finish(maperr());
 int r=stream_inode_allowed(ni,0);if(r<0)return read_finish(r);
 ntfs_attr*na=open_named_data(ni,name,units);if(!na)return read_finish(maperr());
 if(na->data_size<0||na->initialized_size<0||na->initialized_size>na->data_size)return read_finish(BAD);
 if(na->data_flags&ATTR_IS_ENCRYPTED)return read_finish(UNSUP);
 if(off>(uint64_t)na->data_size||(uint64_t)n>(uint64_t)na->data_size-off)return read_finish(INVALID);
 if(n&&ntfs_attr_pread(na,(s64)off,n,b)!=n)return read_finish(maperr());return read_finish(0);
}
int multi_ntfs3g_stream_change(const char *path,const char *stream,int op,uint64_t off,const u8*b,u32 n){
 ntfschar name[255];u32 units;TRY(stream_name16(stream,name,&units));
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 ntfs_inode*ni=path_inode(v,path);if(!ni)return finish(v,maperr());
 int r=stream_inode_allowed(ni,1);if(r<0)return finish(v,r);
 ntfs_attr*na=open_named_data(ni,name,units);
 if(op==MULTIOS_STREAM_CREATE){
  if(na)return finish(v,EXISTS);if(errno!=ENOENT)return finish(v,maperr());
  if(ntfs_attr_add(ni,AT_DATA,name,(u8)units,0,0))return finish(v,maperr());
  na=open_named_data(ni,name,units);
 }
 if(!na)return finish(v,maperr());
 if(na->data_size<0||na->initialized_size<0||na->initialized_size>na->data_size)return finish(v,BAD);
 if(na->data_flags&ATTR_IS_ENCRYPTED)return finish(v,UNSUP);
 if(op==MULTIOS_STREAM_REMOVE){if(ntfs_attr_rm(na))return finish(v,maperr());ntfs_attr_close(na);na=0;}
 else {
  if(op==MULTIOS_STREAM_REPLACE)r=resize_attribute(na,0);
  else if(op==MULTIOS_STREAM_RESIZE)r=resize_attribute(na,off);
  else if(op==MULTIOS_STREAM_APPEND)off=(uint64_t)na->data_size;
  else if(op==MULTIOS_STREAM_WRITE&&(off>(uint64_t)na->data_size||(uint64_t)n>(uint64_t)na->data_size-off))r=INVALID;
  if(r>=0&&op!=MULTIOS_STREAM_RESIZE&&n){
   if(off>INT64_MAX||(uint64_t)n>INT64_MAX-off)r=INVALID;
   else if(ntfs_attr_pwrite(na,(s64)off,n,b)!=n)r=maperr();
  }
  if(r<0||io_error<0)return finish(v,r);
  if(ntfs_attr_pclose(na))return finish(v,maperr());
  if(op==MULTIOS_STREAM_RESIZE&&(uint64_t)na->data_size!=off)return finish(v,BAD);
  ntfs_attr_close(na);
 }
 ntfs_inode_update_times(ni,NTFS_UPDATE_MCTIME);
 if(ntfs_inode_close(ni))return finish(v,maperr());return finish(v,0);
}
int multi_ntfs3g_link(const char *source,const char *destination){
 ntfs_volume*v=open_volume(0);if(!v){int r=maperr();mpntfs_reset();return r;}
 ntfs_inode*ni=path_inode(v,source);if(!ni)return finish(v,maperr());
 int r=safe_inode(ni,0);if(r<0)return finish(v,r);
 ntfs_inode*parent=0;ntfschar name[256];u8 units;
 r=parent_name(v,destination,&parent,name,&units);if(r<0)return finish(v,r);
 ntfs_inode*exists=path_inode(v,destination);
 if(exists)return finish(v,EXISTS);if(errno!=ENOENT)return finish(v,maperr());
 if(ntfs_link(ni,parent,name,units))return finish(v,maperr());
 ntfs_inode_update_times(ni,NTFS_UPDATE_CTIME);
 if(ntfs_inode_close(parent))return finish(v,maperr());
 if(ntfs_inode_close(ni))return finish(v,maperr());return finish(v,0);
}
