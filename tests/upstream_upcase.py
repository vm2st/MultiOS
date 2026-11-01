#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — mutations must use the actual exFAT $UpCase.
from pathlib import Path
import sys,tempfile,subprocess,struct
r=Path(__file__).resolve().parents[1];sys.path[:0]=[str(r/'tests'),str(r/'tools')]
from unicode_filesystems import exmeta,checksum,u16,p16,p32
from mkimage import create
from prepare_journal import prepare
with tempfile.TemporaryDirectory(prefix='custom-upcase-',dir=r/'build') as td:
 td=Path(td);src=td/'source.img';dst=td/'test.img';create(src);b=bytearray(src.read_bytes());root,size,up,start,length=exmeta(b)
 c=off=0;word=None
 while c<65536:
  v=u16(b,start+off);off+=2
  if v==65535 and c!=65535:c+=u16(b,start+off);off+=2
  else:
   if c==0x44f:word=start+off-2
   c+=1
 assert word is not None;p16(b,word,0x410);p32(b,up+4,checksum(b[start:start+length]));src.write_bytes(b);prepare(src,dst,1)
 commands=['unlock YES','write я.txt "CUSTOM-CASE"','type А.txt','append А.txt "SECOND"','type я.txt','lock','exit']
 p=subprocess.run([str(r/'build/multios-host'),str(dst),'--allow-write'],input=('\n'.join(commands)+'\n').encode(),capture_output=True,timeout=120)
 text=p.stdout.decode(errors='replace');assert p.returncode==0 and 'Error:' not in text and 'CUSTOM-CASE' in text and 'SECOND' in text,(text,p.stderr)
 with dst.open('rb') as f:
  m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part=td/'check.part';part.write_bytes(f.read(n*512))
 p=subprocess.run(['fsck.exfat','-n',str(part)],capture_output=True);assert p.returncode==0,(p.stdout,p.stderr)
 (r/'build/upstream-upcase.log').write_text(text+p.stdout.decode(errors='replace'))
print('PASS custom on-volume exFAT $UpCase: create lowercase я, read/append via custom uppercase А; independent fsck.exfat.')
