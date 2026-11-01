#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — standalone UEFI, NOT BIOS NTFS RW.
from pathlib import Path
import sys,subprocess,struct,os
from uefi_smoke import boot
r=Path(__file__).resolve().parents[1];w=r/'build/ntfs3g-uefi-verified';w.mkdir(exist_ok=True)
cfg=(r/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
commands=['fs','unlock YES','md TEST','write TEST/a.txt "NTFS-UEFI"','append TEST/a.txt "APPEND"','copy TEST/a.txt TEST/b.txt','cmp TEST/a.txt TEST/b.txt','ren TEST/b.txt final.txt','type TEST/final.txt','write persisted.txt "UEFI-PERSISTED"','del TEST/final.txt','del TEST/a.txt','rd TEST','lock','poweroff']
text,seconds,image=boot(commands,w,config=cfg,osk=False,source=Path(os.environ.get('MULTIOS_UPSTREAM_IMAGE',r/'build/MultiOS-NTFS3G-UNPUBLISHED.img')),timeout=90)
(w/'console.log').write_text(text)
assert 'Error:' not in text and 'Identical' in text and 'NTFS-UEFI' in text and 'APPEND' in text,text
with image.open('rb') as f:
 m=f.read(512);start,n=struct.unpack_from('<II',m,470);f.seek(start*512);data=f.read(n*512)
p=w/'ntfs.part';p.write_bytes(data)
x=subprocess.run(['ntfscat',str(p),'/persisted.txt'],capture_output=True)
assert x.returncode==0 and x.stdout==b'UEFI-PERSISTED\n',(x.stdout,x.stderr)
print('PASS standalone UEFI NTFS-3G: create/append/copy/compare/rename/delete/mkdir/rmdir; independent ntfscat after poweroff; prompt %.3fs'%seconds)
print('Boot Services retained. NOT native xHCI storage, NOT ExitBootServices kernel. BIOS NTFS remains RO.')
