#!/usr/bin/env python3
"""Real FS write/resize: notification before/after selected raw-write boundaries."""
from pathlib import Path
import os,re,sys,struct,subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from prepare_journal import prepare
from mkimage import create
probe=Path(os.environ.get('MULTIOS_MEDIA_RW_PROBE',root/'build/fs-media-rw-probe'))
def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=180)
 assert p.returncode==0,(args,p.returncode,p.stdout[-1200:],p.stderr[-2000:]);return p.stdout
with tempfile.TemporaryDirectory(prefix='media-rw-',dir=root/'build') as td:
 t=Path(td);total=0
 for kind in ('fat32','exfat','ntfs'):
  source=t/(kind+'.src');base=t/(kind+'.img');part=t/(kind+'.part');image=t/'attempt.img'
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   if kind=='fat32':run('mkfs.fat','-F','32','-s','1',part)
   else:run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),12 if kind=='fat32' else 7,bytes(3),2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,base,1,experimental_ntfs=kind=='ntfs',journal_records=4096);run(probe,base,'seed')
  for op in (0,1):
   shutil.copyfile(base,image);out=run(probe,image,'attempt',0,0,op);n=int(re.search(rb'writes=(\d+)',out)[1]);assert n>0
   points=sorted({1,2,3,4,n,n-1,n-2,*range(1,n+1,max(1,n//12))})
   for step in points:
    for phase in (0,1):
     shutil.copyfile(base,image);run(probe,image,'attempt',step,phase,op);run(probe,image,'recover',op)
     with image.open('rb') as f:
      m=f.read(512);start,size=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(size*512))
     run({'fat32':'fsck.fat','exfat':'fsck.exfat','ntfs':'ntfsfix'}[kind],'-n',part)
     if kind!='exfat':
      data=run('mtype','-i',part,'::DATA.BIN') if kind=='fat32' else run('ntfscat',part,'/DATA.BIN')
      original=bytes((i*37+11)&255 for i in range(32769));changed=bytearray(original)
      if op:changed.extend(bytes(65539-len(changed)))
      else:changed[509:518]=b'PATCH123\0'
      assert data in (original,bytes(changed))
   cases=2*len(points);total+=cases
   print(f'{kind}: op={op} PASS {cases} selected before/after notification cases; {n} baseline raw writes; no post-failure callbacks; reopen, whole old/new DATA, fresh RW and external basic checker',flush=True)
 print(f'PASS VFS media-failure matrix: {total} fixture cases; selected boundaries, not hardware certification',flush=True)
