#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real 1MiB writes/copies; external complete readback.
from pathlib import Path
import tempfile,subprocess,sys,struct
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));from prepare_journal import prepare
from mkimage import create

def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=300);assert not p.returncode,(args,p.stdout[-1000:],p.stderr[-2000:]);return p.stdout
with tempfile.TemporaryDirectory(prefix='capacity-',dir=r/'build') as td:
 td=Path(td)
 for kind in ('fat32','exfat','ntfs'):
  source=td/(kind+'.src');image=td/(kind+'.img');part=td/(kind+'.part')
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   if kind=='fat32':run('mkfs.fat','-F','32','-s','1',part)
   else:run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),12 if kind=='fat32' else 7,bytes(3),2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,image,1,experimental_ntfs=kind=='ntfs',journal_records=4096)
  print(kind+': '+run(r/'build/fs-capacity-probe',image).decode().strip(),flush=True)
  with image.open('rb') as f:
   m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
  expected=bytes((i*17+(i>>8))&255 for i in range(1024*1024))
  if kind=='fat32':assert run('mtype','-i',part,'::renamed-megabyte.bin')==expected
  if kind=='ntfs':assert run('ntfscat',part,'/renamed-megabyte.bin')==expected
  run('fsck.fat' if kind=='fat32' else 'fsck.exfat' if kind=='exfat' else 'ntfsfix','-n',part)
print('PASS all three filesystems: configured 4096-sector journal, full 1MiB contents; independent FAT32/NTFS readback and external basic checkers.')
