#!/usr/bin/env python3
"""Independent read-only checks on disposable images left by USB QEMU tests.
Usage: python3 tests/usb_result_check.py IMAGE FS
Requires fsck.fat, fsck.exfat, ntfsfix, mtype, ntfscat as appropriate.
ntfsfix -n is NOT Windows chkdsk certification.
"""
from pathlib import Path
import sys,struct,subprocess,tempfile

def verify(image,fs):
 image=Path(image)
 with image.open('rb') as f:
  m=f.read(512);base,count=struct.unpack_from('<II',m,470);f.seek(base*512);raw=f.read(count*512)
 assert len(raw)==count*512
 with tempfile.TemporaryDirectory(prefix='usb-fs-check-') as td:
  p=Path(td)/'data.part';p.write_bytes(raw)
  cmd={'fat32':['fsck.fat','-n'],'exfat':['fsck.exfat','-n'],'ntfs':['ntfsfix','-n']}[fs]
  result=subprocess.run([*cmd,str(p)],capture_output=True)
  assert not result.returncode,(result.stdout,result.stderr)
  if fs=='fat32':
   result=subprocess.run(['mtype','-i',str(p),'::/PERSIST.TXT'],capture_output=True);assert not result.returncode
   content=result.stdout
  elif fs=='ntfs':
   result=subprocess.run(['ntfscat',str(p),'/PERSIST.TXT'],capture_output=True);assert not result.returncode
   content=result.stdout
  else:
   # Minimal independent extractor for this ASCII root-file fixture only.
   u32=lambda o:struct.unpack_from('<I',raw,o)[0]
   sec=1<<raw[108];spc=1<<raw[109];cs=sec*spc;fat=u32(80)*sec;heap=u32(88)*sec
   def cluster(c):
    start=heap+(c-2)*cs;assert c>=2 and start+cs<=len(raw);return raw[start:start+cs]
   def chain(first,contiguous=False,length=0):
    c=first;seen=set();data=bytearray()
    while c<0xfffffff8:
     assert c not in seen and len(seen)<4096;seen.add(c);data.extend(cluster(c))
     if length and len(data)>=length:break
     c=c+1 if contiguous else struct.unpack_from('<I',raw,fat+c*4)[0]
    return bytes(data)
   directory=chain(u32(96));content=None
   for off in range(0,len(directory),32):
    e=directory[off:off+32]
    if not e[0]:break
    if e[0]!=0x85:continue
    following=directory[off+32:off+32*(e[1]+1)];stream=following[:32]
    if len(stream)!=32 or stream[0]!=0xc0:continue
    name=b''.join(following[j+2:j+32] for j in range(32,len(following),32) if following[j]==0xc1)
    name=name[:stream[3]*2].decode('utf-16le')
    if name.casefold()=='persist.txt':
     first=struct.unpack_from('<I',stream,20)[0];length=struct.unpack_from('<Q',stream,24)[0];assert 0<length<1024
     content=chain(first,bool(stream[1]&2),length)[:length];break
  assert content==b'alpha\nbeta\n',content
 print(f'PASS independent {fs}: read-only checker and exact PERSIST.TXT bytes: {image.name}',flush=True)
if __name__=='__main__':verify(sys.argv[1],sys.argv[2])
