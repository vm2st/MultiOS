#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — run after building upstream-growth FatFs profile.
from pathlib import Path
import subprocess,sys,struct,shutil,tempfile
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from mkimage import create
from prepare_journal import prepare
logs=[]
def cmd(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=180);assert p.returncode==0,(a,p.stdout[-2000:],p.stderr[-2000:]);logs.append((p.stdout+p.stderr).decode(errors='replace'));return p.stdout
with tempfile.TemporaryDirectory(prefix='upstream-growth-',dir=r/'build') as td:
 td=Path(td)
 for kind in ('fat32','exfat'):
  source=td/(kind+'-source.img');image=td/(kind+'-test.img');part=td/(kind+'.part')
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   cmd('mkfs.fat','-F','32','-s','1',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,b'\0'*3,0x0c,b'\0'*3,2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,image,1);print(cmd(r/'build/upstream-growth',image).decode().strip(),flush=True)
  with image.open('rb') as f:
   m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
  cmd('fsck.fat' if kind=='fat32' else 'fsck.exfat','-n',part)
  if kind=='fat32':assert cmd('mtype','-i',part,'::large.bin')==bytes(range(256))*640+b'TAIL'
(r/'build/upstream-growth.log').write_text('\n'.join(logs));print('PASS independent fsck after directory growth and incremental large-file appends; FAT32 data independently compared.')
