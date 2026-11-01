#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — independent case-sensitive NTFS ADS fixture.
from pathlib import Path
import subprocess,tempfile,ctypes as c,ctypes.util,os,sys,struct
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_journal import prepare
from ntfs_fixture import populate
def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=180);assert not p.returncode,(args,p.stdout[-1000:],p.stderr[-1000:]);return p.stdout
with tempfile.TemporaryDirectory(prefix='ads-casefold-',dir=r/'build') as td:
 t=Path(td);part=t/'ntfs.part';source=t/'source.img';image=t/'run.img'
 with part.open('wb') as f:f.truncate(64*1024**2)
 run('mkntfs','-F','-Q','-s','512','-c','4096',part);populate(part)
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;U=c.c_uint32;L=c.c_int64
 def fn(n,result,args):f=getattr(lib,n);f.restype=result;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);add=fn('ntfs_attr_add',I,[P,U,P,c.c_uint8,P,L]);close=fn('ntfs_inode_close',I,[P]);unmount=fn('ntfs_umount',I,[P,I])
 v=mount(os.fsencode(part),0x08000000);assert v;ni=inode(v,None,b'/resident.txt');assert ni
 for name in ('CaseClash','caseclash'):
  encoded=c.create_string_buffer(name.encode('utf-16le'));assert add(ni,0x80,encoded,len(name),c.create_string_buffer(b'SAME'),4)==0,c.get_errno()
 assert close(ni)==0 and unmount(v,0)==0
 for name in ('CaseClash','caseclash'):assert run('ntfscat','-n',name,part,'/resident.txt')==b'SAME'
 m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),7,bytes(3),2048,part.stat().st_size//512)
 with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
 prepare(source,image,1,experimental_ntfs=True,journal_records=4096)
 p=subprocess.run([str(r/'build/multios-host'),str(image),'--allow-write'],input=b'unlock YES\ncopyads /resident.txt /BAD.TXT\nlock\nexit\n',capture_output=True,timeout=180)
 text=p.stdout.decode(errors='replace');assert not p.returncode and 'Error:' in text and 'Staging retained:' in text and 'Copied and verified DATA + ADS' not in text,text
 with image.open('rb') as f:f.seek(2048*512);part.write_bytes(f.read(64*1024**2))
 for name in ('CaseClash','caseclash'):assert run('ntfscat','-n',name,part,'/resident.txt')==b'SAME'
 absent=subprocess.run(['ntfscat',str(part),'/BAD.TXT'],capture_output=True);assert absent.returncode!=0
 run('ntfsfix','-n',part)
 print('PASS independent case-sensitive ADS fixture: two casefold-equal names with IDENTICAL bytes; COPYADS refuses publication, preserves both source streams, retains staging; external ntfscat/ntfsfix.')
