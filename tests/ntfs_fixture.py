#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — test-only client of installed libntfs-3g.
# Does not distribute or link this library into MultiOS. Refuses raw devices.
from pathlib import Path
import ctypes as c, ctypes.util, stat, os

def populate(path):
 path=Path(path);s=path.lstat()
 assert stat.S_ISREG(s.st_mode) and s.st_nlink==1 and not path.is_symlink()
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True)
 def fn(n,restype,args):
  f=getattr(lib,n);f.restype=restype;f.argtypes=args;return f
 ptr=c.c_void_p;i=c.c_int;ll=c.c_int64;u=c.c_uint32;uchar=c.c_uint8
 mount=fn('ntfs_mount',ptr,[c.c_char_p,c.c_ulong]);unmount=fn('ntfs_umount',i,[ptr,i])
 inode=fn('ntfs_pathname_to_inode',ptr,[ptr,ptr,c.c_char_p]);close=fn('ntfs_inode_close',i,[ptr])
 close_in=fn('ntfs_inode_close_in_dir',i,[ptr,ptr])
 create=fn('ntfs_create',ptr,[ptr,u,ptr,uchar,u]);symlink=fn('ntfs_create_symlink',ptr,[ptr,u,ptr,uchar,ptr,i]);link=fn('ntfs_link',i,[ptr,ptr,ptr,uchar])
 attr=fn('ntfs_attr_open',ptr,[ptr,u,ptr,u]);aclose=fn('ntfs_attr_close',None,[ptr]);write=fn('ntfs_attr_pwrite',ll,[ptr,ll,ll,ptr]);truncate=fn('ntfs_attr_truncate',i,[ptr,ll])
 def ok(value):
  if not value:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return value
 def good(value):
  if value<0:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return value
 def name(s):return c.create_string_buffer(s.encode('utf-16le'))
 vol=ok(mount(os.fsencode(path),0x08000000));root=ok(inode(vol,None,b'/'))
 def new(parent,n,directory=False):return ok(create(parent,0,name(n),len(n),stat.S_IFDIR if directory else stat.S_IFREG))
 def put(ni,data,offset=0):
  a=ok(attr(ni,0x80,None,0))
  try:assert good(write(a,offset,len(data),c.create_string_buffer(data)))==len(data)
  finally:aclose(a)
 try:
  d=new(root,'DOCS',True);n=new(d,'Nested.txt');put(n,b'Nested NTFS directory\n');good(close_in(n,d));good(close_in(d,root))
  n=new(root,'resident.txt');put(n,b'Resident NTFS\n');good(link(n,root,name('hardlink.txt'),len('hardlink.txt')));good(close_in(n,root))
  n=new(root,'fragmented.bin');put(n,b'A'*4096)
  blocker=new(root,'blocker.bin');put(blocker,b'B'*131072);good(close_in(blocker,root));put(n,b'C'*8192,4096);good(close_in(n,root))
  n=new(root,'sparse.bin');a=ok(attr(n,0x80,None,0));good(truncate(a,1048576));aclose(a);put(n,b'SPARSE-END',1048576-10);good(close_in(n,root))
  n=new(root,'uninitialized.bin');a=ok(attr(n,0x80,None,0));good(truncate(a,16384));aclose(a);good(close_in(n,root))
  n=ok(symlink(root,0,name('linkdir'),len('linkdir'),name('DOCS'),len('DOCS')));good(close_in(n,root))
  n=new(root,'Unicode_\u041f\u0440\u0438\u0432\u0435\u0442.txt');put(n,b'Unicode metadata\n');good(close_in(n,root))
 finally:
  good(close(root));good(unmount(vol,0))

if __name__=='__main__':
 import sys
 populate(sys.argv[1]);print('Independent libntfs-3g: nested directory, resident/hardlink, nonresident/fragmentation candidate, sparse/uninitialized/reparse and Unicode fixtures created.')
