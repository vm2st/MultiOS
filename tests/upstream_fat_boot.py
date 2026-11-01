#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — final combined EFI on both FAT data filesystems.
from pathlib import Path
import os,struct,subprocess,sys
from uefi_smoke import boot
r=Path(__file__).resolve().parents[1];kind=sys.argv[1];assert kind in ('fat32','exfat')
w=r/('build/upstream-uefi-'+kind);cfg=(r/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
commands=['ver','fs','unlock YES','md TEST','write TEST/a.txt "FAT-UEFI"','append TEST/a.txt "APPEND"','copy TEST/a.txt TEST/b.txt','cmp TEST/a.txt TEST/b.txt','ren TEST/b.txt final.txt','type TEST/final.txt','write persisted.txt "UEFI-PERSISTED"','del TEST/final.txt','del TEST/a.txt','rd TEST','lock','poweroff']
text,seconds,image=boot(commands,w,config=cfg,osk=False,source=Path(os.environ['MULTIOS_UPSTREAM_IMAGE']),timeout=90)
(w/'console.log').write_text(text);assert 'Error:' not in text and 'Identical' in text and 'FAT-UEFI' in text and 'APPEND' in text,text
with image.open('rb') as f:
 m=f.read(512);start,n=struct.unpack_from('<II',m,470);f.seek(start*512);part=w/'data.part';part.write_bytes(f.read(n*512))
p=subprocess.run(['fsck.fat' if kind=='fat32' else 'fsck.exfat','-n',str(part)],capture_output=True);assert p.returncode==0,(p.stdout,p.stderr)
if kind=='fat32':assert subprocess.check_output(['mtype','-i',str(part),'::persisted.txt'])==b'UEFI-PERSISTED\n'
print('PASS standalone UEFI '+kind+': create/append/copy/compare/rename/delete/mkdir/rmdir, independent fsck after poweroff; prompt %.3fs'%seconds)
