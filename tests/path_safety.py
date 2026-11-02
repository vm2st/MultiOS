#!/usr/bin/env python3
"""Real partition images, VFS/HAL mutations and independent basic checkers."""
from pathlib import Path
import os,struct,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from prepare_journal import prepare
from mkimage import create
probe=Path(os.environ.get('MULTIOS_PATH_PROBE',root/'build/fs-path-probe'))
def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=120)
 if p.returncode: raise RuntimeError((args,p.returncode,p.stdout.decode(errors='replace')[-1000:],p.stderr.decode(errors='replace')[-2000:]))
 return p.stdout
with tempfile.TemporaryDirectory(prefix='path-safety-',dir=root/'build') as td:
 t=Path(td)
 for kind in sys.argv[1:] or ('fat32','exfat','ntfs'):
  source=t/(kind+'.src');image=t/(kind+'.img');part=t/(kind+'.part')
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   if kind=='fat32':run('mkfs.fat','-F','32','-s','1',part)
   else:run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),12 if kind=='fat32' else 7,bytes(3),2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,image,1,experimental_ntfs=kind=='ntfs',journal_records=int(os.environ.get('MULTIOS_PATH_JOURNAL_RECORDS','4096')))
  print(kind+': '+run(probe,image).decode().strip(),flush=True)
  with image.open('rb') as f:
   m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
  run({'fat32':'fsck.fat','exfat':'fsck.exfat','ntfs':'ntfsfix'}[kind],'-n',part)
  if os.environ.get('MULTIOS_PATH_REPRO'):continue
  if kind=='fat32':assert run('mtype','-i',part,'::KEEP.TXT')==b'KEEP!'
  if kind=='ntfs':assert run('ntfscat',part,'/KEEP.TXT')==b'KEEP!'
  print(kind+': PASS independent basic checker'+ (' and independent byte read' if kind!='exfat' else ''),flush=True)
