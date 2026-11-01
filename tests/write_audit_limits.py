#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — own writes must remain reopenable within depth cap.
from pathlib import Path
import tempfile,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 image=Path(td)/'deep.img';shutil.copyfile(ROOT/'build/multios.img',image)
 paths=['/'+'/'.join('D'+str(j) for j in range(1,i+1)) for i in range(1,18)]
 commands=['unlock YES',*['md '+p for p in paths],'lock','unlock YES',f'write {paths[15]}/OK.TXT "deep"',f'type {paths[15]}/OK.TXT','exit']
 p=subprocess.run([str(ROOT/'build/multios-host'),str(image),'--allow-write'],input='\n'.join(commands)+'\n',text=True,capture_output=True,timeout=30)
 assert p.returncode==0 and p.stdout.count('Error:')==1 and p.stdout.count('EXPERIMENTAL WRITE ENABLED')==2 and '\ndeep\n' in p.stdout,p.stdout
 data=Path(td)/'data.exfat'
 with image.open('rb') as f:f.seek(2048*512);data.write_bytes(f.read())
 q=subprocess.run(['fsck.exfat','-n',str(data)],capture_output=True,text=True);assert q.returncode==0,q.stdout+q.stderr
report='PASS depth boundary: directories 1..16 created, depth 17 rejected before mutation, LOCK/re-UNLOCK succeeds, deepest file write/read and independent fsck clean.\n'
(ROOT/'build/dev10-write-limits-report.txt').write_text(report);print(report)
