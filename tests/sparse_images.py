# vm2 studios | t.me/v2vproj — test-only sparse ordinary-image utilities.
from pathlib import Path
import os,hashlib,struct,errno,stat

def ranges(path):
 p=Path(path);assert p.is_file() and not p.is_symlink();size=p.stat().st_size
 with p.open('rb') as f:
  at=0
  while at<size:
   try:start=os.lseek(f.fileno(),at,os.SEEK_DATA)
   except OSError as e:
    if e.errno==errno.ENXIO:return
    raise
   end=min(size,os.lseek(f.fileno(),start,os.SEEK_HOLE));assert at<=start<end;yield start,end;at=end

def digest(path):
 p=Path(path);h=hashlib.sha256();h.update(struct.pack('<Q',p.stat().st_size))
 with p.open('rb') as f:
  for a,b in ranges(p):
   h.update(struct.pack('<QQ',a,b-a));f.seek(a)
   while a<b:data=f.read(min(1024*1024,b-a));assert data;h.update(data);a+=len(data)
 return h.digest()

def copy_range(source,dest,offset=0):
 source=Path(source);dest=Path(dest)
 assert source.resolve()!=dest.resolve() and source.is_file() and not source.is_symlink()
 assert dest.is_file() and not dest.is_symlink() and dest.stat().st_nlink==1
 with source.open('rb') as a,dest.open('r+b') as b:
  b.truncate(offset+source.stat().st_size)
  for start,end in ranges(source):
   a.seek(start);b.seek(offset+start)
   while start<end:data=a.read(min(1024*1024,end-start));assert data;b.write(data);start+=len(data)

def copy_image(source,dest):
 # Repeated test runs may replace an old ordinary output, never devices/links.
 p=Path(dest)
 if p.exists():assert p.is_file() and not p.is_symlink() and p.stat().st_nlink==1;p.unlink()
 with p.open('xb'):pass
 copy_range(source,p)
 return str(p)
