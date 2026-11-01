#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — sparse host image, real exFAT >5GiB metadata.
from pathlib import Path
import sys,subprocess,tempfile,struct,os,ctypes as c,ctypes.util,stat
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from mkimage import entry,checksum
from prepare_journal import prepare
probe=Path(os.environ.get('MULTIOS_RANDOM_PROBE',r/'build/fs-random-probe'))
def run(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=600);assert p.returncode==0,(a,p.returncode,p.stderr[-3000:],p.stdout[-1000:]);return p.stdout
size=5*1024**3+17
with tempfile.TemporaryDirectory(prefix='wide-random-',dir=r/'build') as td:
 t=Path(td);part=t/'data.part';source=t/'source.img';image=t/'tx.img'
 for kind in ('exfat','ntfs'):
  with part.open('wb') as f:f.truncate(8*1024**3 if kind=='exfat' else 96*1024**2)
  if kind=='exfat':
   run('mkfs.exfat','-c','65536',part)
   with part.open('r+b') as f:
    b=f.read(512);heap,nc,root=struct.unpack_from('<III',b,88);cs=512<<b[109]
    def pos(cl):return heap*512+(cl-2)*cs
    f.seek(pos(root));d=bytearray(f.read(cs));bm=next(i for i in range(0,cs,32) if d[i]==0x81);bmfirst=struct.unpack_from('<I',d,bm+20)[0];bmlen=struct.unpack_from('<Q',d,bm+24)[0];f.seek(pos(bmfirst));bits=bytearray(f.read(bmlen))
    first=100;count=(size+cs-1)//cs;assert first+count<=nc+2
    for cl in range(first,first+count):assert not(bits[(cl-2)//8]&(1<<((cl-2)%8)));bits[(cl-2)//8]|=1<<((cl-2)%8)
    off=next(i for i in range(0,cs,32) if not d[i]);e=entry('WIDE.BIN',first,size,flags=3);d[off:off+len(e)]=e
    f.seek(pos(root));f.write(d);f.seek(pos(bmfirst));f.write(bits)
    # PercentInUse is excluded from the boot checksum.
    for bo in (0,12*512):
     f.seek(bo);region=bytearray(f.read(11*512));struct.pack_into('<Q',region,64,2048);region[112]=255;f.seek(bo);f.write(region);f.write(struct.pack('<I',checksum(region,(106,107,112)))*128)
  else:
   run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;L=c.c_int64
   def fn(n,res,args):f=getattr(lib,n);f.restype=res;f.argtypes=args;return f
   mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);create=fn('ntfs_create',P,[P,c.c_uint32,P,c.c_uint8,c.c_uint32]);attr=fn('ntfs_attr_open',P,[P,c.c_uint32,P,c.c_uint32]);trunc=fn('ntfs_attr_truncate',I,[P,L]);aclose=fn('ntfs_attr_close',None,[P]);close=fn('ntfs_inode_close_in_dir',I,[P,P]);iclose=fn('ntfs_inode_close',I,[P]);umount=fn('ntfs_umount',I,[P,I])
   v=mount(os.fsencode(part),0x08000000);assert v;root=inode(v,None,b'/');assert root;n=create(root,0,c.create_string_buffer('WIDE.BIN'.encode('utf-16le')),8,stat.S_IFREG);assert n;a=attr(n,0x80,None,0);assert a;assert trunc(a,size)==0;aclose(a);assert close(n,root)==0;assert iclose(root)==0;assert umount(v,0)==0
  m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),7,bytes(3),2048,part.stat().st_size//512)
  with source.open('wb') as f:f.write(m);f.truncate(2048*512+part.stat().st_size)
  # Sparse-aware copy, no 8GiB RAM materialization.
  with part.open('rb') as src,source.open('r+b') as dst:
   off=0
   while off<part.stat().st_size:
    try:start=os.lseek(src.fileno(),off,os.SEEK_DATA)
    except OSError:break
    end=os.lseek(src.fileno(),start,os.SEEK_HOLE);src.seek(start);dst.seek(2048*512+start)
    while start<end:b=src.read(min(1024**2,end-start));dst.write(b);start+=len(b)
    off=end
  prepare(source,image,1,experimental_ntfs=kind=='ntfs',journal_records=4096)
  print(kind+': '+run(probe,image,'wide').decode().strip(),flush=True)
  # Read final partition sparsely using cp then dd --sparse to preserve disk space.
  with image.open('rb') as src,part.open('wb') as dst:
   dst.truncate((8*1024**3) if kind=='exfat' else 96*1024**2);start=2048*512;end=start+part.stat().st_size
   while start<end:
    try:off=os.lseek(src.fileno(),start,os.SEEK_DATA)
    except OSError:break
    if off>=end:break
    stop=min(end,os.lseek(src.fileno(),off,os.SEEK_HOLE));src.seek(off);dst.seek(off-2048*512)
    while off<stop:
     b=src.read(min(1024**2,stop-off))
     # prepare() can materialize zero extents. Re-extract those as holes;
     # logical bytes are identical and the independent checker sees all zeros.
     if b.strip(b'\0'):dst.write(b)
     else:dst.seek(len(b),1)
     off+=len(b)
    start=stop
  run('fsck.exfat' if kind=='exfat' else 'ntfsfix','-n',part);print(kind+': PASS independent basic checker after >4GiB random write/resize',flush=True)
  source.unlink();image.unlink();part.unlink()
