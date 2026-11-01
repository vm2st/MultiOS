#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — bounded 64-bit console operations, source immutability.
from pathlib import Path
import os,hashlib,subprocess,struct
R=Path(__file__).resolve().parents[1];W=R/'build/fs64-tests';logs=[];checks=0

from sparse_images import digest
def check(x,msg):
 global checks
 checks+=1
 if not x:raise AssertionError(msg)
for kind in ('fat32','exfat','exfat-chain','ntfs'):
 p=W/(kind+'.img');before=digest(p);size=4294901760 if kind=='fat32' else 5368709243
 commands=['stat64 wide.bin','readat wide.bin 0 10','hexat wide.bin 0 10','readat wide.bin 0 0','readat wide.bin '+str(size)+' 0']
 if kind!='fat32':commands+=['readat wide.bin 0xfffffff8 16','hexat wide.bin 0x100000000 8','readat wide.bin 0x1000001ff 9','readat wide.bin '+str(size-16)+' 16']
 else:commands+=['readat wide.bin '+str(size-16)+' 16']
 invalid=['readat wide.bin -1 1','readat wide.bin 18446744073709551616 1','readat wide.bin 0x10000000000000000 1','readat wide.bin 0 4097','readat wide.bin 0 0x100000000','readat wide.bin 0x 1','readat wide.bin 0xffffffffffffffff 2','readat wide.bin '+str(size)+' 1','readat wide.bin 0 +1','stat64 /']
 for exe in ('multios-host','multios-sanitize-host'):
  run=subprocess.run([str(R/'build'/exe),str(p)],input='\n'.join(commands+invalid+['exit'])+'\n',capture_output=True,text=True,timeout=120);text=run.stdout+run.stderr;logs.append(kind+' / '+exe+'\n'+text)
  check(run.returncode==0,text);check(text.count('Error:')==len(invalid),text);check('AddressSanitizer' not in text and 'runtime error:' not in text,text)
  check('Size64: '+str(size) in text and 'WIDE-START' in text,text)
  if kind!='fat32':check('CROSS-4G-BOUNDAR' in text and '0000000100000000' in text and 'SECTOR-64' in text and 'END-OF-WIDE-FILE' in text,text)
  else:check('FAT32-NEAR-LIMIT' in text,text)
 check(digest(p)==before,kind+' altered source sparse extent bytes/layout')
(R/'build/fs64-console.log').write_text('\n'.join(logs));(R/'build/fs64-console-report.txt').write_text(f'PASS {checks} console checks across FAT32/exFAT-contiguous/exFAT-chained/NTFS, normal + ASan/UBSan. STAT64/READAT/HEXAT, decimal/hex, >4GiB offsets, strict integer overflow/EOF/count cap, source sparse extents unchanged.\n');print((R/'build/fs64-console-report.txt').read_text())
