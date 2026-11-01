#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — repeated real BIOS recovery-process interruptions.
from pathlib import Path
import tempfile,struct,sys,shutil
from bios_rw_boot import boot,extract,run
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from build_bios_rw_image import install

def configure(image,at):
 with image.open('r+b') as f:f.seek(17*512+16);f.write(struct.pack('<II',at,1 if at else 0))
with tempfile.TemporaryDirectory(prefix='mirror-bios-',dir=r/'build') as td:
 t=Path(td)
 for kind in ('fat32','exfat','ntfs'):
  w=t/kind;w.mkdir();text,_,seed=boot(r/('build/022-'+kind+'.img'),w/'seed',['unlock YES','write CHAIN.TXT "ORIGINAL"','lock','exit'])
  assert 'Error:' not in text,text
  current=install(seed,w/'fault.img',r/'build/bios-rw-fault')
  # Begin(2 writes), first before-image+descriptor+mirrors(4), target(1).
  # Stop before write 8, with one logged/possibly changed target sector.
  configure(current,8);cmd='write CHAIN.TXT "CHANGED"'
  text,_,current=boot(current,w/'initial',['unlock YES',cmd],expected_exit=35,crash_command=cmd)
  assert 'BIOS TEST PROCESS STOP' in text,text
  # Restore one target, write first CLEAN, stop before second CLEAN.
  configure(current,3);text,_,current=boot(current,w/'recovery',[],startup_exit=35)
  assert 'BIOS TEST PROCESS STOP' in text,text
  # Four more begin/recovery pairs with alternating newest mirror positions.
  for n in range(4):
   configure(current,2);text,_,current=boot(current,w/f'begin{n}',['unlock YES',cmd],expected_exit=35,crash_command=cmd)
   assert 'BIOS TEST PROCESS STOP' in text,text
   configure(current,2);text,_,current=boot(current,w/f'recover{n}',[],startup_exit=35)
   assert 'BIOS TEST PROCESS STOP' in text,text
  configure(current,0);text,_,current=boot(current,w/'final',['fs','type CHAIN.TXT','unlock YES','write VERIFIED.TXT "AFTER RECOVERY"','lock','exit'])
  assert 'Error:' not in text and 'degraded mirrors' in text and '\nORIGINAL\n' in text,text
  part=w/'data.part';extract(current,part);run('ntfsfix' if kind=='ntfs' else 'fsck.fat' if kind=='fat32' else 'fsck.exfat','-n',part)
  if kind=='ntfs':assert run('ntfscat',part,'/CHAIN.TXT')==b'ORIGINAL\n' and run('ntfscat',part,'/VERIFIED.TXT')==b'AFTER RECOVERY\n'
  elif kind=='fat32':assert run('mtype','-i',part,'::CHAIN.TXT')==b'ORIGINAL\n' and run('mtype','-i',part,'::VERIFIED.TXT')==b'AFTER RECOVERY\n'
  print('PASS native BIOS '+kind+': 10 consecutive process stops incl dirty-sector recovery and repeated split CLEAN/ACTIVE mirrors; new RW transaction + external checker.',flush=True)
print('PASS 3 FS x 10 BIOS process stops. QEMU/cache model only, not physical power loss.')
