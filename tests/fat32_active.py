#!/usr/bin/env python3
"""Active FAT32 regressions on disposable files. Requires mtools/dosfstools.
Usage: fat32_active.py BASE_IMAGE [--prepare-only] [--faults]
"""
from pathlib import Path
import subprocess,struct,sys,hashlib,shutil
r=Path(__file__).resolve().parents[1];w=r/'build/active-fat30';w.mkdir(parents=True,exist_ok=True)
def run(args,**kw):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600,**kw)
 assert not p.returncode,(args,p.stdout[-4000:],p.stderr[-4000:]);return p.stdout
base=Path(sys.argv[1]);assert base.is_file() and not base.is_symlink()
seed=w/'seeded.img';shutil.copyfile(base,seed)
raw=bytearray(seed.read_bytes());start,count=struct.unpack_from('<II',raw,470);offset=start*512
u16=lambda b,o:struct.unpack_from('<H',b,o)[0]
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
def setup(image):
 drive=str(image)+'@@'+str(offset)
 for name,data in [('SEED.BIN',b'S'*8193),('old.txt',b'original\n')]:
  p=w/name;p.write_bytes(data);run(['mcopy','-o','-i',drive,p,'::'+name])
 run(['mmd','-i',drive,'::empty'])
setup(seed);plain=seed.read_bytes();b=plain[offset:offset+512];reserved=u16(b,14);fat_size=u32(b,36);backup=u16(b,50)
assert b[16]==2 and backup and u16(b,11)==512
cases=[]
for selected in (0,1):
 raw=bytearray(plain);flags=0x80|selected
 for boot in (0,backup):struct.pack_into('<H',raw,offset+boot*512+40,flags)
 inactive=start+reserved+(1-selected)*fat_size
 raw[inactive*512:(inactive+fat_size)*512]=b'\xa5'*(fat_size*512)
 p=w/f'active{selected}.img';p.write_bytes(raw);cases.append((p,inactive,fat_size,selected))
# One physical FAT is also valid; keep partition geometry/journal binding unchanged.
part=w/'single.part';part.write_bytes(bytes(count*512));run(['mkfs.fat','-F','32','-s','1','-f','1',part])
raw=bytearray(plain);raw[offset:offset+count*512]=part.read_bytes();single=w/'single.img';single.write_bytes(raw);setup(single)
raw=bytearray(single.read_bytes());bk=u16(raw,offset+50)
for boot in (0,bk):struct.pack_into('<H',raw,offset+boot*512+40,0x80)
single.write_bytes(raw);cases.append((single,0,0,0))
if '--prepare-only' in sys.argv:print('Prepared active0.img, active1.img, single.img');sys.exit(0)
for original,inactive,fsiz,selected in cases:
 before=original.read_bytes();region=before[inactive*512:(inactive+fsiz)*512]
 for binary in ('fat32-active-probe','fat32-active-probe-sanitize'):
  p=w/'working.img';p.write_bytes(before)
  print(original.stem+'/'+binary+': '+run([r/'build'/binary,p,inactive,fsiz,'read']).decode().strip(),flush=True)
  assert p.read_bytes()==before
  print(original.stem+'/'+binary+': '+run([r/'build'/binary,p,inactive,fsiz,'write']).decode().strip(),flush=True)
  after=p.read_bytes();assert after[inactive*512:(inactive+fsiz)*512]==region
  # Independent tools receive a read-only verification projection: selected table
  # copied to both slots and mirror flag cleared. The actual image is NOT repaired.
  data=bytearray(after[offset:offset+count*512]);reserved=u16(data,14);fat_size=u32(data,36);bk=u16(data,50)
  active=data[(reserved+selected*fat_size)*512:(reserved+(selected+1)*fat_size)*512]
  for k in range(data[16]):data[(reserved+k*fat_size)*512:(reserved+(k+1)*fat_size)*512]=active
  for boot in (0,bk):struct.pack_into('<H',data,boot*512+40,0)
  part=w/'verify.part';part.write_bytes(data)
  run(['fsck.fat','-n',part]);expect=bytearray(b'S'*8193);expect[509:514]=b'PATCH';expect[513:]=bytes(8193-513)
  assert run(['mtype','-i',part,'::RENAMED.BIN'])==expect
  print('PASS independent selected-FAT projection: fsck -n + exact bytes; inactive original unchanged',flush=True)
# Guards: invalid selected table, unknown flags, mismatched mirror, inconsistent backup.
reserved=u16(plain,offset+14);fat_size=u32(plain,offset+36)
for label,flags,mismatch in [('bad-index',0x82,False),('reserved-flags',0x180,False),('backup-disagrees',0x81,True),('mirror-disagrees',0,False)]:
 raw=bytearray(plain)
 for boot in (0,backup):struct.pack_into('<H',raw,offset+boot*512+40,0 if mismatch and boot else flags)
 if label=='mirror-disagrees':raw[offset+(reserved+fat_size)*512+200]^=1
 p=w/(label+'.img');p.write_bytes(raw);before=hashlib.sha256(raw).digest()
 print(label+': '+run([r/'build/fat32-active-probe-sanitize',p,0,0,'deny']).decode().strip(),flush=True)
 assert hashlib.sha256(p.read_bytes()).digest()==before
if '--faults' in sys.argv:
 for selected in (0,1):
  original=w/f'active{selected}.img'
  for binary in ('storage-tx-test','storage-tx-sanitize'):
   text=run([r/'build'/binary,original]).decode();(w/f'active{selected}-{binary}.log').write_text(text)
   print('active'+str(selected)+'/'+binary+': '+text.splitlines()[-1],flush=True)
print('PASS active FAT0/FAT1 and single-FAT read/write, inactive byte preservation, guards and independent projection checks. Not full FAT32/hardware certification.')
