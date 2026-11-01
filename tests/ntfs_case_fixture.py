#!/usr/bin/env python3
"""Seed existing POSIX case-colliding NTFS files, directories and named DATA.
Uses the system libntfs-3g on an ordinary temporary partition file, not the
production MultiOS adapter. Does not enable Windows directory case flags.
"""
import ctypes as c,ctypes.util,os,stat

def seed(part):
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;L=c.c_int64
 def fn(name,ret,args):f=getattr(lib,name);f.restype=ret;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);umount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);close=fn('ntfs_inode_close',I,[P]);create=fn('ntfs_create',P,[P,c.c_uint32,P,c.c_uint8,c.c_uint32]);aopen=fn('ntfs_attr_open',P,[P,c.c_uint32,P,c.c_uint32]);aclose=fn('ntfs_attr_close',None,[P]);add=fn('ntfs_attr_add',I,[P,c.c_uint32,P,c.c_uint8,P,L]);write=fn('ntfs_attr_pwrite',L,[P,L,L,P]);pclose=fn('ntfs_attr_pclose',I,[P])
 v=mount(os.fsencode(part),0x08000000);assert v,c.get_errno()
 root=inode(v,None,b'/');assert root
 def make(parent,name,data=None):
  n=c.create_string_buffer(name.encode('utf-16le'));units=len(name.encode('utf-16le'))//2
  ni=create(parent,0,n,units,stat.S_IFREG if data is not None else stat.S_IFDIR);assert ni,(name,c.get_errno())
  if data is not None:
   a=aopen(ni,0x80,None,0);assert a;buf=c.create_string_buffer(data);assert write(a,0,len(data),buf)==len(data);assert not pclose(a);aclose(a)
  return ni
 for name,data in [('File.txt',b'A'*8193),('file.txt',b'b'*8197),('old.txt',b'old fixture'),('Unique.TXT',b'unique'),('Stream.bin',b'BASE')]:
  ni=make(root,name,data)
  if name=='Stream.bin':
   for stream,content in [('Notes',b'FIRST'),('notes',b'second-long'),('Only',b'UNIQUE')]:
    n=c.create_string_buffer(stream.encode('utf-16le'));assert not add(ni,0x80,n,len(stream),None,0),(stream,c.get_errno())
    a=aopen(ni,0x80,n,len(stream));assert a;assert write(a,0,len(content),c.create_string_buffer(content))==len(content);assert not pclose(a);aclose(a)
  assert not close(ni)
 for name,data in [('CaseDir',b'U'*4097),('casedir',b'd'*8193)]:
  parent=make(root,name);child=make(parent,'item.bin',data);assert not close(parent);assert not close(child)
 assert not close(root);assert not umount(v,0)
