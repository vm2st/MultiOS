#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj
from pathlib import Path
import sys,shutil
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));import customize as c
from bios_rw_boot import boot
from uefi_smoke import boot as efi_boot
w=r/'build/025-native';w.mkdir(exist_ok=True)
for fs in ('fat32','exfat','ntfs'):
 src=r/('build/025-editor-tests/'+fs+'-safe.img')
 t,_,img=boot(src,w/('bios-'+fs),['ver','modules','osk','unlock YES','write FORBID.TXT "NO"','exit']);(w/('bios-'+fs+'.log')).write_text(t)
 assert 'block: pc-ata-pio-ro' in t and 'Error:' in t and 'OSK OFF' in t and 'Available compiled modules' in t,t
 assert img.read_bytes()==src.read_bytes();print('PASS BIOS '+fs+': selected RO profile; unlock/write refused; disk unchanged',flush=True)
 t,_,img=efi_boot(['ver','modules','osk','unlock YES','write FORBID.TXT "NO"','poweroff'],w/('efi-'+fs),source=src,osk=False,timeout=120);(w/('efi-'+fs+'.log')).write_text(t)
 assert 'block: uefi-boot-disk-ro' in t and 'Error:' in t and 'OSK OFF' in t,t
 assert img.read_bytes()==src.read_bytes();print('PASS UEFI '+fs+': selected RO profile; unlock/write refused; disk unchanged',flush=True)
src=w/'two-buttons.img';src.unlink(missing_ok=True);c.apply(r/'build/025-fat32.img',src,c.PRESETS['two-buttons'])
t,_,_=boot(src,w/'bios-osk-guard',[],guard_message='OSK | F6: Next');assert 'OSK | F6: Next' in t
t,_,_=boot(src,w/'bios-osk',['\x0fosk','osk on','\x0fosk','exit']);assert t.count('OSK OFF')==2 and 'Error:' not in t;tpath=w/'bios-osk.log';tpath.write_text(t)
t,_,_=efi_boot(['modules','osk','poweroff'],w/'efi-osk',source=src,osk=True,screen=w/'efi-osk.ppm',timeout=120);assert 'portable C core' in t and 'OSK OFF' in t;(w/'efi-osk.log').write_text(t)
print('PASS BIOS/UEFI OSK preset: VGA panel, toggles/command, UEFI arrow-only VER',flush=True)
bad=w/'corrupt.img';shutil.copyfile(src,bad)
with bad.open('r+b') as f:f.seek(8192);f.write(b'?')
_,_,img=boot(bad,w/'bios-corrupt',[],guard_message='invalid BIOS configuration');assert img.read_bytes()==bad.read_bytes()
print('PASS BIOS corrupt config: fail-stop, disk unchanged',flush=True)
with bad.open('r+b') as f:f.seek(8192);f.write(bytes(512))
t,_,_=boot(bad,w/'bios-empty',['modules','osk','exit']);assert 'block: pc-ata-pio -' in t and 'OSK OFF' in t
print('PASS BIOS empty slot: compiled fallback',flush=True)
