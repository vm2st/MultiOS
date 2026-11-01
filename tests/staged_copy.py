#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real-FS staged copy and selected raw-fault boundaries.
from pathlib import Path
import sys,subprocess,tempfile,struct,os,re,shutil
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_journal import prepare
from mkimage import create
probe=Path(os.environ.get('MULTIOS_COPY_PROBE',r/'build/fs-copy-probe'))
def run(*args,expect=0):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600)
 assert p.returncode==expect,(args,p.returncode,p.stdout[-1500:],p.stderr[-3000:])
 return p.stdout

def extract(image,part):
 with image.open('rb') as f:
  m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))

def check(kind,image,part):
 extract(image,part);run('fsck.fat' if kind=='fat32' else 'fsck.exfat' if kind=='exfat' else 'ntfsfix','-n',part)

with tempfile.TemporaryDirectory(prefix='staged-copy-',dir=r/'build') as td:
 t=Path(td);payload=bytes(range(256))*(4*1024*1024//256)+bytes(range(123));bulk=t/'bulk.bin';bulk.write_bytes(payload)
 for kind in ('fat32','exfat','ntfs'):
  source=t/(kind+'.src');base=t/(kind+'.img');part=t/(kind+'.part')
  if kind=='exfat':create(source)
  else:
   with part.open('wb') as f:f.truncate(64*1024*1024)
   if kind=='fat32':run('mkfs.fat','-F','32','-s','1',part)
   else:run('mkntfs','-F','-Q','-s','512','-c','4096',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),12 if kind=='fat32' else 7,bytes(3),2048,part.stat().st_size//512)
   with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
  prepare(source,base,1,experimental_ntfs=kind=='ntfs',journal_records=int(os.environ.get('MULTIOS_TEST_JOURNAL_RECORDS','4096')))
  print(kind+': '+run(probe,base,'seed',bulk).decode().strip(),flush=True)
  image=t/'suite.img';shutil.copyfile(base,image)
  print(kind+': '+run(probe,image,'suite').decode().strip(),flush=True);check(kind,image,part)
  if kind=='fat32':assert run('mtype','-i',part,'::LARGE.BIN')==payload
  if kind=='ntfs':assert run('ntfscat',part,'/LARGE.BIN')==payload
  shutil.copyfile(base,image);baseline=run(probe,image,'attempt',0,0);writes=int(re.search(rb'writes=(\d+)',baseline)[1])
  # Selected boundaries, not an exhaustive physical power-loss certificate.
  points=sorted({1,2,3,4,5,6,7,8,writes,writes-1,writes-2,writes-3,writes-4,*range(1,writes+1,max(1,writes//20))})
  for step in points:
   for crash in (0,1):
    shutil.copyfile(base,image);run(probe,image,'attempt',step,crash,expect=77 if crash else 0)
    run(probe,image,'recover');check(kind,image,part)
  print(f'{kind}: PASS {len(points)} selected raw-write boundaries x I/O failure/process termination; recovery+resume+external basic checker each; baseline {writes} sector writes.',flush=True)
print('PASS staged copy all 3 FS; >4 MiB DATA copy (not metadata clone), restart/resume and selected failures. Model, not hardware certification.',flush=True)
