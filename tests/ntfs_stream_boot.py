#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — standalone UEFI NTFS DATA/ADS/hardlink regression.
from pathlib import Path
import subprocess,struct,re
from uefi_smoke import boot
r=Path(__file__).resolve().parents[1];w=r/'build/022-ntfs-stream-boot';w.mkdir(exist_ok=True)
cfg=(r/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
commands=['ver','fs','unlock YES','write BASE.TXT "MAIN"','snew BASE.TXT Notes "abcdef"','swriteat BASE.TXT notes 2 "XY"','sappend BASE.TXT NOTES "!"','sresize BASE.TXT Notes 16','streams BASE.TXT','shex BASE.TXT notes 0 16','link BASE.TXT ALIAS.TXT','copyads ALIAS.TXT COPIED.TXT','swrite ALIAS.TXT notes "SHARED"','sread BASE.TXT Notes 0 6','del BASE.TXT','sread ALIAS.TXT Notes 0 6','sdel ALIAS.TXT Notes','streams ALIAS.TXT','cmp ALIAS.TXT COPIED.TXT','lock','poweroff']
text,seconds,image=boot(commands,w,config=cfg,osk=False,source=r/'build/022-ntfs.img',timeout=180)
(w/'console.log').write_text(text)
assert 'Error:' not in text and text.count('SHARED')>=3 and 'Copied and verified DATA + ADS' in text and 'Identical' in text,text[-8000:]
assert '61 62 58 59 65 66 21 00 00 00 00 00 00 00 00 00' in text,text[-5000:]
with image.open('rb') as f:
 m=f.read(512);start,n=struct.unpack_from('<II',m,470);f.seek(start*512);part=w/'data.part';part.write_bytes(f.read(n*512))
def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True);assert not p.returncode,(args,p.stdout,p.stderr);return p.stdout
main=run('ntfscat',part,'/ALIAS.TXT');assert run('ntfscat',part,'/COPIED.TXT')==main
assert run('ntfscat','-n','Notes',part,'/COPIED.TXT')==b'abXYef!'+bytes(9)
info=run('ntfsinfo','-F','/ALIAS.TXT',part);assert re.search(rb'Number of Hard Links:\s+1',info)
assert b'Notes' not in info
run('ntfsfix','-n',part)
print(f'PASS standalone UEFI NTFS: ADS create/casefold/write/append/resize/list/read/delete; true hardlink/shared ADS/unlink; COPYADS preserves bytes; external ntfscat/ntfsinfo/ntfsfix after poweroff. Prompt {seconds:.3f}s (emulator only).')
