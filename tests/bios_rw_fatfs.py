#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj
from bios_rw_boot import r,boot,extract,run
for kind in ('fat32','exfat'):
 commands=['unlock YES','write TEST.TXT "DATA"','append TEST.TXT "TAIL"','resize TEST.TXT 8193','writeat TEST.TXT 4095 "PATCH"','resize TEST.TXT 10','copy TEST.TXT COPY.TXT','cmp TEST.TXT COPY.TXT','ren COPY.TXT RENAMED.TXT','md EMPTY','rd EMPTY','lock','exit']
 w=r/'build'/('022-bios-'+kind);text,seconds,image=boot(r/('build/022-'+kind+'.img'),w,commands)
 assert 'Error:' not in text,text
 part=w/'data.part';extract(image,part)
 run('fsck.fat' if kind=='fat32' else 'fsck.exfat','-n',part)
 if kind=='fat32':assert run('mtype','-i',part,'::RENAMED.TXT')==b'DATA\nTAIL\n'
 print('PASS native BIOS '+kind+': write/append/resize/write-at/copy/compare/rename/directories; external fsck. Not hardware certification.',flush=True)
