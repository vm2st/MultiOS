#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — exhaustive selected-operation write/flush models.
from pathlib import Path
import subprocess,tempfile,shutil,os
r=Path(__file__).resolve().parents[1]
# Raw old-or-new comparison requires identical NTFS timestamp bytes across
# baseline and later fault attempts; production host clock is unchanged.
os.environ['MULTIOS_NTFS_TEST_TIME']='1'
probe=Path(os.environ.get('MULTIOS_TX_PROBE',r/'build/storage-tx-test'))
log_prefix=os.environ.get('MULTIOS_TX_LOG_PREFIX','023-matrix')
with tempfile.TemporaryDirectory(prefix='writeguard-fs-',dir=r/'build') as td:
 for kind in ('fat32','exfat','ntfs'):
  image=Path(td)/(kind+'.img');shutil.copyfile(r/('build/022-'+kind+'.img'),image)
  p=subprocess.run([str(r/'build/multios-host'),str(image),'--allow-write'],input=b'unlock YES\nwrite old.txt "original"\nmd empty\nlock\nexit\n',capture_output=True,timeout=180)
  assert p.returncode==0 and b'Error:' not in p.stdout,(kind,p.stdout,p.stderr)
  p=subprocess.run([str(probe),str(image)],capture_output=True,timeout=1200)
  (r/('build/'+log_prefix+'-'+kind+'.log')).write_bytes(p.stdout+p.stderr)
  assert p.returncode==0,(kind,p.stdout[-2000:],p.stderr[-2000:])
  print(kind+': '+p.stdout.decode().split('PASS real-FS private transactions:')[1].strip(),flush=True)
print('PASS all three real FS exhaustive write/flush boundaries for eight mutation types, recovery interruption and budget guards; cached/torn/partial/flush-complete models, not hardware certification.')
