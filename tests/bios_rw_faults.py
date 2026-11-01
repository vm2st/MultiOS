#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — selected native BIOS raw-write failures/recovery.
from pathlib import Path
import sys,struct,re,tempfile,shutil,subprocess
from bios_rw_boot import boot,extract,run
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from build_bios_rw_image import install
with tempfile.TemporaryDirectory(prefix='bios-native-faults-',dir=r/'build') as td:
 t=Path(td)
 text,_,seed=boot(r/'build/022-ntfs.img',t/'seed',['unlock YES','write TX.TXT "ORIGINAL"','snew TX.TXT Data "ADSOLD"','lock','exit'])
 assert 'Error:' not in text,text
 fault=install(seed,t/'fault.img',r/'build/bios-rw-fault')
 ops=['writeat TX.TXT 1 "PATCH"','resize TX.TXT 8193','swriteat TX.TXT Data 1 "NEW"','link TX.TXT LINK.TXT','copyads TX.TXT COPIED.TXT']
 for i,cmd in enumerate(ops):
  text,_,image=boot(fault,t/'baseline',['unlock YES',cmd,'testraw','lock','exit'])
  assert 'Error:' not in text,text;count=int(re.search(r'TEST raw writes=(\d+)',text)[1]);assert count>0
  points=sorted({1,2,max(3,count//2),count})
  for at in points:
   for mode in (0,1):
    src=t/'attempt.img';shutil.copyfile(fault,src)
    with src.open('r+b') as f:f.seek(17*512+16);f.write(struct.pack('<II',at,mode))
    if mode==0:
     text,_,failed=boot(src,t/'attempt',['unlock YES',cmd,'testraw'],stop_at='testraw')
     assert 'BIOS TEST sticky raw I/O error' in text and 'Error:' in text,text
    else:
     text,_,failed=boot(src,t/'attempt',['unlock YES',cmd],expected_exit=35,crash_command=cmd)
     assert 'BIOS TEST PROCESS STOP' in text,text
    with failed.open('r+b') as f:f.seek(17*512+16);f.write(bytes(8))
    text,_,recovered=boot(failed,t/'recover',['fs','exit']);assert 'Error:' not in text and 'NTFS mounted' in text,text
    part=t/'check.part';extract(recovered,part);run('ntfsfix','-n',part)
    data=run('ntfscat',part,'/TX.TXT');ads=run('ntfscat','-n','Data',part,'/TX.TXT')
    expected=b'O' + b'PATCH' + b'AL\n'
    assert data in ([b'ORIGINAL\n',expected] if i==0 else [b'ORIGINAL\n',b'ORIGINAL\n'+bytes(8193-9)] if i==1 else [b'ORIGINAL\n']),(i,at,mode,len(data),data[:20])
    assert ads in ([b'ADSOLD',b'ANEWLD'] if i==2 else [b'ADSOLD']),(i,at,mode,ads)
    if i in (3,4):
     name='/LINK.TXT' if i==3 else '/COPIED.TXT';p=subprocess.run(['ntfscat',str(part),name],capture_output=True)
     if not p.returncode:assert p.stdout==b'ORIGINAL\n' and run('ntfscat','-n','Data',part,name)==b'ADSOLD'
  print(f'PASS native BIOS op={i}: {len(points)} selected raw-write boundaries x sticky I/O failure/process stop; fresh BIOS recovery; external ntfscat+ntfsfix; baseline {count} writes.',flush=True)
print('PASS BIOS native failure model: 40 selected attempts + fresh native recovery, not hardware power-loss certification.')
