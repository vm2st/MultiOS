#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj
from pathlib import Path
import sys,struct,hashlib,shutil
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));import customize as c
w=r/'build/025-editor-tests';w.mkdir(exist_ok=True)
def digest(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  while b:=f.read(1024**2):h.update(b)
 return h.digest()
def fails(fn):
 try:fn()
 except (ValueError,OSError,UnicodeError):return
 raise AssertionError('Expected refusal')
for fs in ('fat32','exfat','ntfs'):
 src=r/('build/025-'+fs+'.img');before=digest(src)
 with c.open_image(src) as f:esp,_=c.inspect_file(f)
 ranges=[(8192,8704)]
 for path in c.FILES:ranges.extend((1048576+o,1048576+o+len(b)) for o,b in esp.patches(path,bytes(512)))
 for name,s in c.PRESETS.items():
  dst=w/(fs+'-'+name+'.img');dst.unlink(missing_ok=True);c.apply(src,dst,s)
  with src.open('rb') as a,dst.open('rb') as b:
   off=0
   while x:=a.read(65536):
    y=b.read(len(x))
    if x!=y:assert all(any(lo<=off+i<hi for lo,hi in ranges) for i,(v,z) in enumerate(zip(x,y)) if v!=z)
    off+=len(x)
   assert not b.read()
  fails(lambda:c.apply(src,dst,s))
  if name!='safe':dst.unlink()
 assert digest(src)==before
 print('PASS editor '+fs+': four presets, only four config sectors changed, source unchanged, overwrite refused',flush=True)
src=r/'build/025-fat32.img';bad=w/'bad.img';out=w/'bad-output.img'
def corrupt(off,b):
 shutil.copyfile(src,bad)
 with bad.open('r+b') as f:f.seek(off);f.write(b)
 out.unlink(missing_ok=True);fails(lambda:c.apply(bad,out,c.DEFAULT));assert not out.exists()
for o in (8192,17*512+24,18*512+100,src.stat().st_size-1):corrupt(o,b'?')
with c.open_image(src) as f:
 esp,_=c.inspect_file(f);foot=c.read_at(f,src.stat().st_size-1024,512)
corrupt(1048576+esp.offset(esp.files[c.FILES[0]][0][0]),b'osk=on\n')
corrupt(c.u64(foot,24)*512+20,b'\x01')
shutil.copyfile(src,bad)
with bad.open('r+b') as f:
 b=c.read_at(f,1048576,512);res=struct.unpack_from('<H',b,14)[0];nf=c.u32(b,36);cc=esp.files[c.FILES[0]][0][0];other=esp.files[c.FILES[1]][0][0]
 for j in range(2):f.seek(1048576+(res+j*nf)*512+cc*4);f.write(struct.pack('<I',other))
fails(lambda:c.apply(bad,out,c.DEFAULT));assert not out.exists()
shutil.copyfile(src,bad)
with bad.open('r+b') as f:
 o=c.u64(foot,24)*512;h=bytearray(c.read_at(f,o,512));h[88]^=1;struct.pack_into('<I',h,508,c.crc(h[:508]));f.seek(o);f.write(h);f.write(h)
fails(lambda:c.apply(bad,out,c.DEFAULT));assert not out.exists()
link=w/'symlink.img';link.unlink(missing_ok=True);link.symlink_to(src);fails(lambda:c.apply(link,out,c.DEFAULT));link.unlink()
fails(lambda:c.apply('/dev/null',out,c.DEFAULT))
for text in ('osk=on\nosk=off','osk=maybe','unknown=x','osk=on\nbios_input=auto','osk=off\nbios_input=auto\nstorage=unsafe'):
 fails(lambda:c.settings(text))
usb=dict(c.DEFAULT,bios_disk='usb-bot');dst=w/'usb-profile.img';dst.unlink(missing_ok=True);c.apply(src,dst,usb)
with c.open_image(dst) as f:_,actual=c.inspect_file(f)
assert actual==usb;dst.unlink()
fails(lambda:c.settings('osk=off\nbios_input=auto\nstorage=guarded\nbios_disk=bogus\n'))
assert c.settings(' osk = off \r\nbios_input=auto\r\nstorage=guarded\r\n')==c.DEFAULT
bad.unlink(missing_ok=True)
print('PASS editor refusal: corrupt config/header/kernel/footer, mirrors, dirty/foreign journal, FAT cross-link, symlink/device, invalid settings; CRLF/legacy settings and USB disk profile accepted; no failed output left')
