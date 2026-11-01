#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — independent libntfs-3g fixture builder, not OS code.
from pathlib import Path
import ctypes as c,ctypes.util,os,stat

def populate_lists(path,fragment_runs=0):
 p=Path(path);s=p.lstat();assert stat.S_ISREG(s.st_mode) and s.st_nlink==1
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;U=c.c_uint32;L=c.c_int64
 def fn(n,result,args):f=getattr(lib,n);f.restype=result;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);umount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);close=fn('ntfs_inode_close',I,[P]);sync=fn('ntfs_inode_sync',I,[P])
 addlist=fn('ntfs_inode_add_attrlist',I,[P]);ctxget=fn('ntfs_attr_get_search_ctx',P,[P,P]);ctxput=fn('ntfs_attr_put_search_ctx',None,[P]);lookup=fn('ntfs_attr_lookup',I,[U,P,U,I,L,P,U,P]);move=fn('ntfs_attr_record_move_away',I,[P,I])
 add=fn('ntfs_attr_add',I,[P,U,P,c.c_uint8,P,L]);attr=fn('ntfs_attr_open',P,[P,U,P,U]);aclose=fn('ntfs_attr_close',None,[P]);write=fn('ntfs_attr_pwrite',L,[P,L,L,P]);create=fn('ntfs_create',P,[P,U,P,c.c_uint8,U])
 def ok(v):
  if not v:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return v
 def good(v):
  if v<0:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return v
 def name(n):return c.create_string_buffer(n.encode('utf-16le'))
 v=ok(mount(os.fsencode(p),0x08000000));root=ok(inode(v,None,b'/'))
 try:
  for filename,many in [('resident.txt',False),('fragmented.bin',True)]:
   ni=ok(inode(v,None,('/'+filename).encode()))
   try:
    good(addlist(ni));ctx=ok(ctxget(ni,None))
    try:
     good(lookup(0x80,None,0,0,0,None,0,ctx));good(move(ctx,0))
    finally:ctxput(ctx)
    if many:
     for i in range(120):
      n=f'FixtureStream{i:03d}';value=c.create_string_buffer(b'x')
      good(add(ni,0x80,name(n),len(n),value,1))
    good(sync(ni))
   finally:good(close(ni))
  if fragment_runs:
   n1='split.bin';n2='split-blocker.bin';a=ok(create(root,0,name(n1),len(n1),stat.S_IFREG));b=ok(create(root,0,name(n2),len(n2),stat.S_IFREG))
   aa=ok(attr(a,0x80,None,0));bb=ok(attr(b,0x80,None,0))
   try:
    for i in range(fragment_runs):
     payload=c.create_string_buffer(bytes([i%251])*4096);blocker=c.create_string_buffer(bytes([(i+1)%251])*4096)
     assert good(write(aa,i*4096,4096,payload))==4096
     assert good(write(bb,i*4096,4096,blocker))==4096
   finally:aclose(aa);aclose(bb);good(close(a));good(close(b))
 finally:good(close(root));good(umount(v,0))
if __name__=='__main__':
 import sys
 populate_lists(sys.argv[1],int(sys.argv[2]) if len(sys.argv)>2 else 0)
