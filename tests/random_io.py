#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real FS random IO + bounded failure model.
from pathlib import Path
import sys,subprocess,tempfile,struct,os,re,shutil
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_journal import prepare
from mkimage import create,entry,setcheck
probe=Path(os.environ.get('MULTIOS_RANDOM_PROBE',r/'build/fs-random-probe'))
def run(*args,expect=0):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600)
 assert p.returncode==expect,(args,p.returncode,p.stdout[-1500:],p.stderr[-3000:])
 return p.stdout
def extract(image,part):
 with image.open('rb') as f:
  m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
def check(kind,image,part):
 extract(image,part);run('fsck.fat' if kind=='fat32' else 'fsck.exfat' if kind=='exfat' else 'ntfsfix','-n',part)
def poison_exfat(path):
 # Independent on-disk fixture. Nonzero physical bytes past valid length MUST NOT escape.
 with path.open('r+b') as f:
  base=2048*512;f.seek(base);boot=f.read(512);heap,nc=struct.unpack_from('<II',boot,88);cs=512<<boot[109]
  def pos(c):return base+heap*512+(c-2)*cs
  f.seek(pos(2));root=bytearray(f.read(8*cs));bmfirst=struct.unpack_from('<I',root,52)[0];f.seek(pos(bmfirst));bm=bytearray(f.read(cs))
  first=100;root_off=192;fixtures=[]
  for i in range(4):
   size=8193 if i<3 else 3*1024*1024;name=f'UNINIT{i+1 if i else ""}.BIN';n=(size+cs-1)//cs
   data=bytearray(b'\xa5'*(n*cs));data[:13]=b'VALID-PREFIX!';f.seek(pos(first));f.write(data)
   for c in range(first,first+n):bm[(c-2)//8]|=1<<((c-2)%8)
   e=entry(name,first,size,flags=3);struct.pack_into('<Q',e,40,13);struct.pack_into('<H',e,2,setcheck(e));root[root_off:root_off+len(e)]=e
   fixtures.append((name,first,size,root_off));root_off+=len(e);first+=n
  f.seek(pos(2));f.write(root);f.seek(pos(bmfirst));f.write(bm)
 return fixtures
with tempfile.TemporaryDirectory(prefix='random-io-',dir=r/'build') as td:
 t=Path(td)
 for kind in ('fat32','exfat','ntfs'):
  source=t/(kind+'.src');base=t/(kind+'.img');part=t/(kind+'.part')
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   if kind=='fat32':run('mkfs.fat','-F','32','-s','1',part)
   else:run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),12 if kind=='fat32' else 7,bytes(3),2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,base,1,experimental_ntfs=kind=='ntfs',journal_records=4096)
  run(probe,base,'seed');image=t/'suite.img';shutil.copyfile(base,image)
  print(kind+': '+run(probe,image,'suite').decode().strip(),flush=True);check(kind,image,part)
  for op in (0,1):
   shutil.copyfile(base,image);out=run(probe,image,'attempt',0,0,op);writes=int(re.search(rb'writes=(\d+)',out)[1])
   points=sorted({1,2,3,4,writes,writes-1,writes-2,*range(1,writes+1,max(1,writes//12))})
   for step in points:
    for crash in (0,1):
     shutil.copyfile(base,image);run(probe,image,'attempt',step,crash,op,expect=77 if crash else 0);run(probe,image,'recover',op);check(kind,image,part)
   print(f'{kind}: op={op} PASS {len(points)} raw-write boundaries x I/O failure/process termination; {writes} baseline writes',flush=True)
 # Keep raw poisoned source separate from prepared TX clone.
 source=t/'vld.src';create(source);fixtures=poison_exfat(source);image=t/'vld.img';prepare(source,image,1,journal_records=4096)
 print(run(probe,image,'vld').decode().strip(),flush=True);check('exfat',image,t/'vld.part')
 # Check physical tail, not just read API zero masking. First 3 must now have full valid data.
 with image.open('rb') as f:
  f.seek(2048*512);b=f.read(512);heap=struct.unpack_from('<I',b,88)[0];cs=512<<b[109]
  def pos(c):return (2048+heap)*512+(c-2)*cs
  for i,(name,first,size,off) in enumerate(fixtures):
   f.seek(pos(2)+off);e=f.read(96);valid=struct.unpack_from('<Q',e,40)[0];length=struct.unpack_from('<Q',e,56)[0]
   if i<3:
    assert valid==length;f.seek(pos(first)+13);assert f.read(length-13)==(bytes(length-13-4)+b'TAIL' if i==0 else bytes(length-13))
   else:
    assert valid==13 and length==size;f.seek(pos(first)+13);assert f.read(128)==b'\xa5'*128
 print('PASS independent raw exFAT metadata/data verification; no stale bytes published',flush=True)
print('PASS all 3 FS random IO and selected fault model; not hardware certification',flush=True)
