#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — refusal tests; only disposable regular images.
from pathlib import Path
import tempfile,struct,shutil,hashlib,sys
from bios_rw_boot import boot,extract,run
r=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='bios-guard-',dir=r/'build') as td:
 t=Path(td);source=r/'build/022-ntfs.img'
 for kind in ('low-memory','crc','size'):
  image=t/'guard.img';shutil.copyfile(source,image)
  if kind!='low-memory':
   with image.open('r+b') as f:
    if kind=='crc':f.seek(18*512+100);b=f.read(1);f.seek(-1,1);f.write(bytes([b[0]^1]))
    else:f.seek(17*512+8);f.write(struct.pack('<I',0))
  digest=hashlib.sha256(image.read_bytes()).digest();message={'low-memory':'need E820 usable RAM','crc':'BIOS payload CRC mismatch','size':'BIOS EDD payload I/O or size error'}[kind]
  text,_,result=boot(image,t/'vm',[],memory='16M' if kind=='low-memory' else '32M',guard_message=message)
  assert hashlib.sha256(result.read_bytes()).digest()==digest
  print('PASS BIOS '+kind+': rejected before kernel/FS startup; image SHA256 unchanged',flush=True)
 for kind in ('dirty','hibernated'):
  image=t/'unsafe.img';shutil.copyfile(source,image);part=t/'data.part';extract(image,part)
  if kind=='dirty':
   with part.open('r+b') as f:
    bootsector=f.read(512);cluster=struct.unpack_from('<H',bootsector,11)[0]*bootsector[13];record_size=struct.unpack_from('<b',bootsector,64)[0];record_size=(1<<-record_size) if record_size<0 else record_size*cluster
    mft,mirror=struct.unpack_from('<QQ',bootsector,48);f.seek(mft*cluster+3*record_size);record=bytearray(f.read(record_size));off,count=struct.unpack_from('<HH',record,4);seq=record[off:off+2]
    for j in range(1,count):assert record[j*512-2:j*512]==seq;record[j*512-2:j*512]=record[off+j*2:off+j*2+2]
    attr=struct.unpack_from('<H',record,20)[0]
    while struct.unpack_from('<I',record,attr)[0]!=0x70:
     n=struct.unpack_from('<I',record,attr+4)[0];assert n>=24;attr+=n;assert attr<record_size
    v=attr+struct.unpack_from('<H',record,attr+20)[0];flags=struct.unpack_from('<H',record,v+10)[0];struct.pack_into('<H',record,v+10,flags|1)
    for j in range(1,count):record[off+j*2:off+j*2+2]=record[j*512-2:j*512];record[j*512-2:j*512]=seq
    for lcn in (mft,mirror):f.seek(lcn*cluster+3*record_size);f.write(record)
  else:
   payload=t/'hiber';payload.write_bytes(b'hibr'+bytes(4092));run('ntfscp',part,payload,'/hiberfil.sys')
  with image.open('r+b') as f:m=f.read(512);start=struct.unpack_from('<I',m,470)[0];f.seek(start*512);f.write(part.read_bytes())
  digest=hashlib.sha256(image.read_bytes()).digest();text,_,result=boot(image,t/'unsafe-vm',['unlock YES','write DENIED.TXT "NO"','exit'])
  assert 'Error:' in text and 'EXPERIMENTAL WRITE ENABLED' not in text,text
  assert hashlib.sha256(result.read_bytes()).digest()==digest
  print('PASS BIOS '+kind+': RW unlock/mutation denied; full image SHA256 unchanged',flush=True)
print('PASS BIOS loader and unsafe-NTFS guards; synthetic fixtures, not full Windows/hardware certification.')
