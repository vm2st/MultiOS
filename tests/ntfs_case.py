#!/usr/bin/env python3
"""NTFS existing POSIX case collisions: actual VFS/NTFS-3G, no mock FS.
Usage: ntfs_case.py BASE_NTFS_IMAGE [--prepare-only|--before]
"""
from pathlib import Path
import subprocess,struct,sys,shutil,hashlib,os
from ntfs_case_fixture import seed
root=Path(__file__).resolve().parents[1];work=Path(os.environ.get('MULTIOS_CASE_WORK',str(root/'build/ntfs-case32')));work.mkdir(parents=True,exist_ok=True)
def run(args,ok=True):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600)
 if ok:assert not p.returncode,(args,p.returncode,p.stdout[-3000:],p.stderr[-3000:])
 return p
source=Path(sys.argv[1]);assert source.is_file() and not source.is_symlink();original=hashlib.sha256(source.read_bytes()).digest()
image=work/'seed.img';raw=bytearray(source.read_bytes());start,count=struct.unpack_from('<II',raw,470);part=work/'seed.part';part.write_bytes(raw[start*512:(start+count)*512]);seed(part)
for path,expected in [('/File.txt',b'A'*8193),('/file.txt',b'b'*8197),('/CaseDir/item.bin',b'U'*4097),('/casedir/item.bin',b'd'*8193)]:assert run(['ntfscat',part,path]).stdout==expected
for name,data in [('Notes',b'FIRST'),('notes',b'second-long')]:assert run(['ntfscat','-n',name,part,'/Stream.bin']).stdout==data
run(['ntfsfix','-n',part]);raw[start*512:(start+count)*512]=part.read_bytes();image.write_bytes(raw)
assert hashlib.sha256(source.read_bytes()).digest()==original
if '--prepare-only' in sys.argv:print('PASS prepared independent NTFS case-collision fixture');sys.exit(0)
if '--before' in sys.argv:
 p=run([root/'build/ntfs-case-probe',image,'inspect']);print(p.stdout.decode());sys.exit(0)
for binary in ('ntfs-case-probe','ntfs-case-probe-sanitize'):
 for mode in ('read','mutate','guards'):
  temp=work/'working.img';shutil.copyfile(image,temp);before=temp.read_bytes();p=run([root/'build'/binary,temp,mode]);print(binary+'/'+mode+': '+p.stdout.decode().strip(),flush=True)
  after=temp.read_bytes()
  if mode=='read':assert before==after
  part=work/'verify.part';part.write_bytes(after[start*512:(start+count)*512]);run(['ntfsfix','-n',part])
  expected=[('/File.txt',b'A'*8193),('/file.txt',b'b'*8197),('/CaseDir/item.bin',b'U'*4097),('/casedir/item.bin',b'd'*8193)]
  if mode=='mutate':
   upper=bytearray(b'U'*4097);upper[509:514]=b'UPPER';expected=[('/Alias.bin',b'A'*8193),('/ExactCopy.bin',b'A'*8193),('/file.txt',b'LOWER'),('/CaseDir/item.bin',upper),('/casedir/item.bin',b'd'*8193)]
  for path,data in expected:assert run(['ntfscat',part,path]).stdout==data,(binary,mode,path)
  for name,data in [('Notes',b'FIRST'),('notes',b'LOWER-ADS' if mode=='mutate' else b'second-long')]:assert run(['ntfscat','-n',name,part,'/Stream.bin']).stdout==data
  if mode=='guards':assert after[start*512:(start+count)*512]==before[start*512:(start+count)*512]
  print('PASS external ntfscat exact DATA/ADS bytes and ntfsfix -n',flush=True)
print('PASS NTFS case-collision reads/mutations/guards; original input image unchanged. Existing POSIX names, not Windows case-sensitive-directory flags or full NTFS compatibility.')
