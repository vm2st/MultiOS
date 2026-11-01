#!/usr/bin/env python3
"""Near-full real-FS rollback + external read-only check. Ordinary image files only.
Usage: python3 tests/fs_full.py PATH_TO_IMAGES
Build probes first with FS_PROFILE=profiles/filesystems-ntfs3g.mk.
"""
from pathlib import Path
import subprocess,sys,tempfile,struct
r=Path(__file__).resolve().parents[1]
assert len(sys.argv)==2,'Supply a directory with one *-usb-{fat32,exfat,ntfs}.img each'
images=Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='fs-full-',dir=r/'build') as d:
 d=Path(d)
 for fs in ('fat32','exfat','ntfs'):
  found=list(images.glob('*-usb-'+fs+'.img'));assert len(found)==1,(fs,found)
  before=found[0].stat().st_size
  for exe in ('fs-full-probe','fs-full-probe-sanitize'):
   output=d/'result.img'
   p=subprocess.run([str(r/'build'/exe),str(found[0]),str(output)],capture_output=True,text=True,timeout=600)
   assert not p.returncode,(exe,fs,p.stdout[-3000:],p.stderr[-3000:])
   print(fs+'/'+exe+': '+p.stdout.strip(),flush=True)
   with output.open('rb') as f:
    m=f.read(512);start,count=struct.unpack_from('<II',m,470);f.seek(start*512)
    part=d/'data.part';part.write_bytes(f.read(count*512))
   cmd='fsck.fat' if fs=='fat32' else 'fsck.exfat' if fs=='exfat' else 'ntfsfix'
   p=subprocess.run([cmd,'-n',str(part)],capture_output=True,text=True,timeout=120)
   assert not p.returncode,(fs,cmd,p.stdout,p.stderr)
   if fs in ('fat32','ntfs'):
    args=['mtype','-i',str(part),'::KEEP.TXT'] if fs=='fat32' else ['ntfscat',str(part),'/KEEP.TXT']
    p=subprocess.run(args,capture_output=True,timeout=120);assert not p.returncode and p.stdout==b'PRESERVE'+(bytes(262144-8) if fs=='ntfs' else b''),(fs,p.stdout,p.stderr)
   print(fs+'/'+exe+': PASS independent read-only checker'+(' + exact KEEP.TXT' if fs!='exfat' else ''),flush=True)
   output.unlink();part.unlink()
  assert found[0].stat().st_size==before
print('PASS 3 FS x normal/sanitizer: near-full create/append/copy + FAT resize/NTFS sparse materialization refusal, exact rollback and remount. Not hardware certification.')
