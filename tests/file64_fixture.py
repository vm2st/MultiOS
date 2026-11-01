#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — TEST ONLY, regular sparse images, no devices.
from pathlib import Path
import struct,subprocess,ctypes as c,ctypes.util,os,stat,sys
R=Path(__file__).resolve().parents[1];W=R/'build/fs64-tests';W.mkdir(exist_ok=True)
SIZE=5*1024**3+123
MARKERS={0:b'WIDE-START',2**32-8:b'CROSS-4G-BOUNDARY',2**32+511:b'SECTOR-64',SIZE-16:b'END-OF-WIDE-FILE'}
def regular(p,size):
 if p.exists():assert p.is_file() and not p.is_symlink() and p.stat().st_nlink==1;p.unlink()
 with p.open('xb') as f:f.truncate(size)
def run(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=90);assert p.returncode==0,p.stdout+p.stderr;return p.stdout+p.stderr

def ntfs():
 p=W/'ntfs.img';regular(p,64*1024**2);run('mkntfs','-F','-Q','-s','512',p)
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;L=c.c_int64
 def fn(n,t,args):f=getattr(lib,n);f.restype=t;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);unmount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);create=fn('ntfs_create',P,[P,c.c_uint32,P,c.c_uint8,c.c_uint32]);close=fn('ntfs_inode_close',I,[P]);attr=fn('ntfs_attr_open',P,[P,c.c_uint32,P,c.c_uint32]);aclose=fn('ntfs_attr_close',None,[P]);write=fn('ntfs_attr_pwrite',L,[P,L,L,P]);truncate=fn('ntfs_attr_truncate',I,[P,L]);read=fn('ntfs_attr_pread',L,[P,L,L,P])
 v=mount(os.fsencode(p),0x08000000);assert v;root=inode(v,None,b'/');assert root
 ni=create(root,0,c.create_string_buffer('wide.bin'.encode('utf-16le')),8,stat.S_IFREG);assert ni;a=attr(ni,0x80,None,0);assert a
 try:
  assert truncate(a,SIZE)==0,(c.get_errno(),os.strerror(c.get_errno()))
  for off,data in MARKERS.items():assert write(a,off,len(data),c.create_string_buffer(data))==len(data)
  # Independent libntfs-3g oracle, including the actual >4GiB offsets.
  for off in (0,2**32-16,2**32-1,2**32,2**32+500,SIZE-64):
   b=c.create_string_buffer(64);assert read(a,off,64,b)==64
   assert b.raw==expected(off,64,SIZE)
 finally:aclose(a);assert close(ni)==0;assert close(root)==0;assert unmount(v,0)==0
 (W/'ntfs-info.txt').write_bytes(run('ntfsinfo','-f','-v','-F','/wide.bin',p));(W/'ntfs-check.txt').write_bytes(run('ntfsfix','-n',p));return p

def expected(off,n,size):
 b=bytearray(n)
 for pos,data in MARKERS.items():
  if pos>=size:continue
  for k,x in enumerate(data):
   if off<=pos+k<off+n and pos+k<size:b[pos+k-off]=x
 return bytes(b)

def exfat(chain=False,uninit=False):
 p=W/('exfat-uninit.img' if uninit else 'exfat-chain.img' if chain else 'exfat.img');regular(p,8*1024**3);run('mkfs.exfat','-c','32768',p)
 with p.open('r+b') as f:
  b=f.read(512);heap=struct.unpack_from('<I',b,88)[0]*512;fat=struct.unpack_from('<I',b,80)[0]*512;cs=512<<b[109];root=struct.unpack_from('<I',b,96)[0];f.seek(heap+(root-2)*cs);r=bytearray(f.read(cs))
  bitmap=next(i for i in range(0,cs,32) if r[i]==0x81);bm=struct.unpack_from('<I',r,bitmap+20)[0];bmsize=struct.unpack_from('<Q',r,bitmap+24)[0]
  first=16;count=(SIZE+cs-1)//cs;f.seek(heap+(bm-2)*cs);bits=bytearray(f.read(bmsize))
  for cl in range(first,first+count):assert not bits[(cl-2)//8]&(1<<((cl-2)%8));bits[(cl-2)//8]|=1<<((cl-2)%8)
  f.seek(heap+(bm-2)*cs);f.write(bits)
  if chain:
   f.seek(fat+first*4);f.write(b''.join(struct.pack('<I',cl+1 if cl<first+count-1 else 0xffffffff) for cl in range(first,first+count)))
  # Test serializer only; independent fsck.exfat validates the result below.
  sys.path.insert(0,str(R/'tools'));from mkimage import entry,setcheck
  slot=next(i for i in range(0,cs,32) if r[i]==0);data=entry('wide.bin',first,SIZE,1 if chain else 3);
  if uninit:struct.pack_into('<Q',data,40,2**32-4);struct.pack_into('<H',data,2,setcheck(data))
  r[slot:slot+len(data)]=data;f.seek(heap+(root-2)*cs);f.write(r)
  for off,data in MARKERS.items():f.seek(heap+(first-2)*cs+off);f.write(data)
 (W/(p.stem+'-check.txt')).write_bytes(run('fsck.exfat','-n',p));return p

def fat32():
 p=W/'fat32.img';regular(p,8*1024**3);run('mkfs.fat','-F','32','-s','64',p);size=2**32-65536
 with p.open('r+b') as f:
  b=f.read(512);reserved=struct.unpack_from('<H',b,14)[0];fs=struct.unpack_from('<I',b,36)[0];nf=b[16];heap=(reserved+nf*fs)*512;cs=b[13]*512;root=struct.unpack_from('<I',b,44)[0];first=3;count=(size+cs-1)//cs
  table=b''.join(struct.pack('<I',cl+1 if cl<first+count-1 else 0x0fffffff) for cl in range(first,first+count))
  for k in range(nf):f.seek((reserved+k*fs)*512+first*4);f.write(table)
  e=bytearray(32);e[:11]=b'WIDE    BIN';e[11]=32;struct.pack_into('<H',e,26,first);struct.pack_into('<I',e,28,size);f.seek(heap+(root-2)*cs);f.write(e)
  for sec in (1,7):
   f.seek(sec*512+488);f.write(struct.pack('<II',(struct.unpack_from("<I",b,32)[0]-reserved-nf*fs)//b[13]-count-1,first+count))
  for off,data in MARKERS.items():
   if off<size:f.seek(heap+(first-2)*cs+off);f.write(data[:size-off])
   f.seek(heap+(first-2)*cs+size-16);f.write(b'FAT32-NEAR-LIMIT')
 (W/'fat32-check.txt').write_bytes(run('fsck.fat','-n',p));return p
if __name__=='__main__':
 for p in (ntfs(),exfat(False),exfat(True),exfat(False,True),fat32()):print(p,flush=True)
