#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — standalone firmware path, all three FS.
from pathlib import Path
import struct
from uefi_smoke import boot
from bios_rw_boot import extract,run
r=Path(__file__).resolve().parents[1]
cfg=(r/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
for kind in ('fat32','exfat','ntfs'):
 w=r/('build/023-uefi-'+kind);w.mkdir(exist_ok=True)
 commands=['ver','unlock YES','write TEST.TXT "DATA"','append TEST.TXT "TAIL"','resize TEST.TXT 8193','writeat TEST.TXT 4095 "PATCH"','resize TEST.TXT 10','copy TEST.TXT COPY.TXT','cmp TEST.TXT COPY.TXT','ren COPY.TXT RENAMED.TXT','md EMPTY','rd EMPTY','lock','poweroff']
 text,seconds,image=boot(commands,w,config=cfg,osk=False,source=r/('build/022-'+kind+'.img'),timeout=180)
 (w/'console.log').write_text(text);assert 'Error:' not in text and 'Identical' in text,text[-6000:]
 part=w/'data.part';extract(image,part);run('ntfsfix' if kind=='ntfs' else 'fsck.fat' if kind=='fat32' else 'fsck.exfat','-n',part)
 if kind=='fat32':assert run('mtype','-i',part,'::RENAMED.TXT')==b'DATA\nTAIL\n'
 if kind=='ntfs':assert run('ntfscat',part,'/RENAMED.TXT')==b'DATA\nTAIL\n'
 print('PASS standalone UEFI '+kind+': write/append/resize/write-at/copy/compare/rename/directories, external FS checker. QEMU only.',flush=True)
