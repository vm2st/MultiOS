#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real NTFS ADS/hardlinks, independent tools, fault model.
from pathlib import Path
import sys,subprocess,tempfile,struct,os,re,shutil,hashlib,ctypes as c,ctypes.util
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_journal import prepare
from ntfs_fixture import populate
probe=Path(os.environ.get('MULTIOS_STREAM_PROBE',r/'build/ntfs-stream-probe'))
def run(*a,expect=0):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=600)
 assert p.returncode==expect,(a,p.returncode,p.stdout[-2000:],p.stderr[-4000:])
 return p.stdout
def extract(image,part):
 with image.open('rb') as f:m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
def check(image,part):extract(image,part);run('ntfsfix','-n',part)
def external_stream(part):
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;U=c.c_uint32;L=c.c_int64
 def fn(n,result,args):f=getattr(lib,n);f.restype=result;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);add=fn('ntfs_attr_add',I,[P,U,P,c.c_uint8,P,L]);iclose=fn('ntfs_inode_close',I,[P]);umount=fn('ntfs_umount',I,[P,I])
 v=mount(os.fsencode(part),0x08000000);assert v;ni=inode(v,None,b'/resident.txt');assert ni
 name=c.create_string_buffer('External'.encode('utf-16le'));assert add(ni,0x80,name,8,c.create_string_buffer(b'EXT'),3)==0;assert iclose(ni)==0;assert umount(v,0)==0
with tempfile.TemporaryDirectory(prefix='ntfs-streams-',dir=r/'build') as td:
 t=Path(td);part=t/'data.part';source=t/'source.img';base=t/'base.img';image=t/'run.img'
 with part.open('wb') as f:f.truncate(96*1024**2)
 run('mkntfs','-F','-Q','-s','512','-c','4096',part);populate(part);external_stream(part)
 assert run('ntfscat','-n','External',part,'/resident.txt')==b'EXT'
 # Preserve a pre-existing security descriptor and unrelated ADS during mutations.
 secure_before=run('ntfscat','-a','0x50',part,'/resident.txt')
 m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),7,bytes(3),2048,part.stat().st_size//512)
 with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
 prepare(source,base,1,experimental_ntfs=True,journal_records=4096);run(probe,base,'seed');shutil.copyfile(base,image)
 print(run(probe,image,'suite').decode().strip(),flush=True);check(image,part)
 final=run('ntfscat',part,'/FINAL.BIN');assert final[:6]==b'SHARED' and len(final)==32769
 assert run('ntfscat',part,'/DOCS/RENAMED.BIN')==final
 assert run('ntfscat','-n','Keep',part,'/FINAL.BIN')==b'LINK-ADS'
 assert run('ntfscat','-n','External',part,'/resident.txt')==b'EXT'
 assert run('ntfscat','-a','0x50',part,'/resident.txt')==secure_before
 assert run('ntfscat','-n','Ж'*160,part,'/FINAL.BIN')==b'UNICODE'
 assert run('ntfscat','-n','Zone.Identifier',part,'/FINAL.BIN')==b'[ZoneTransfer]\r\nZoneId=3\r\n'
 assert run('ntfscat','-n','DATA',part,'/FINAL.BIN')==bytes((i*37+11)&255 for i in range(13))+bytes(8193-13)
 assert run('ntfscat','-n','DATA',part,'/COPIED.BIN')==bytes((i*37+11)&255 for i in range(13))+bytes(8193-13)
 assert run('ntfscat','-n','Zone.Identifier',part,'/COPIED.BIN')==b'[ZoneTransfer]\r\nZoneId=3\r\n'
 info=run('ntfsinfo','-F','/FINAL.BIN',part).decode();assert re.search(r'Number of Hard Links:\s+2\b',info,re.I),info[:1200]
 print('PASS independent ntfscat: shared unnamed DATA/ADS, long Unicode ADS, Zone.Identifier, unrelated security descriptor bytes, link count=2; ntfsfix -n',flush=True)
 for op in range(6):
  shutil.copyfile(base,image);out=run(probe,image,'attempt',0,0,op);writes=int(re.search(rb'writes=(\d+)',out)[1])
  points=sorted({1,2,3,4,writes,writes-1,writes-2,*range(1,writes+1,max(1,writes//12))})
  for step in points:
   for crash in (0,1):
    shutil.copyfile(base,image);run(probe,image,'attempt',step,crash,op,expect=77 if crash else 0);run(probe,image,'recover',op);check(image,part)
  print(f'PASS NTFS op={op}: {len(points)} selected raw-write boundaries x I/O failure/process termination; {writes} baseline writes; recovery + independent basic checker',flush=True)
 # Test dirty and hibernated refusal using independent on-disk changes.
 for unsafe in ('dirty','hibernated'):
  shutil.copyfile(base,image)
  if unsafe=='dirty':
   with image.open('r+b') as f:
    baseoff=2048*512;f.seek(baseoff);boot=f.read(512);cluster=struct.unpack_from('<H',boot,11)[0]*boot[13];rs=struct.unpack_from('<b',boot,64)[0];rs=(1<<(-rs)) if rs<0 else rs*cluster
    mft,mirror=struct.unpack_from('<QQ',boot,48);f.seek(baseoff+mft*cluster+3*rs);record=bytearray(f.read(rs));off,count=struct.unpack_from('<HH',record,4);seq=record[off:off+2]
    for j in range(1,count):assert record[j*512-2:j*512]==seq;record[j*512-2:j*512]=record[off+j*2:off+j*2+2]
    a=struct.unpack_from('<H',record,20)[0]
    while struct.unpack_from('<I',record,a)[0]!=0x70:
     length=struct.unpack_from('<I',record,a+4)[0];assert length>=24;a+=length;assert a<rs
    value=a+struct.unpack_from('<H',record,a+20)[0];flags=struct.unpack_from('<H',record,value+10)[0];struct.pack_into('<H',record,value+10,flags|1)
    for j in range(1,count):record[off+j*2:off+j*2+2]=record[j*512-2:j*512];record[j*512-2:j*512]=seq
    for lcn in (mft,mirror):f.seek(baseoff+lcn*cluster+3*rs);f.write(record)
  else:
   extract(image,part);payload=t/'hiber';payload.write_bytes(b'hibr'+bytes(4092));run('ntfscp',part,payload,'/hiberfil.sys')
   with image.open('r+b') as f:f.seek(2048*512);f.write(part.read_bytes())
  digest=hashlib.sha256(image.read_bytes()).digest();print(unsafe+': '+run(probe,image,'deny').decode().strip(),flush=True);assert hashlib.sha256(image.read_bytes()).digest()==digest
print('PASS NTFS ADS and hardlinks suite; selected faults are not hardware certification or full Windows compatibility',flush=True)
