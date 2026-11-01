#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — sparse 8 GiB ordinary file, not a physical disk.
from pathlib import Path
import subprocess,os
ROOT=Path(__file__).resolve().parents[1];p=ROOT/'build/fs11-large-fat.img'
with p.open('wb') as f:f.truncate(8*1024**3)
try:
 subprocess.run(['mkfs.fat','-F','32','-s','16',str(p)],check=True,stdout=subprocess.DEVNULL)
 host=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-host'))
 commands='fs\nunlock YES\nwrite large.txt "8 GiB FAT32 volume"\nlock\nunlock YES\ntype large.txt\nexit\n'
 r=subprocess.run([host,str(p),'--allow-write'],input=commands,text=True,capture_output=True,timeout=90)
 assert r.returncode==0 and 'Error:' not in r.stdout and '8 GiB FAT32 volume' in r.stdout,r.stdout+r.stderr
 assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,r.stderr
 s=subprocess.run(['fsck.fat','-n',str(p)],text=True,capture_output=True,timeout=90)
 assert s.returncode==0,s.stdout+s.stderr
 report='8 GiB FAT32 sparse ordinary-image test PASS: ~1 million 8 KiB clusters; UNLOCK/WRITE/LOCK/re-UNLOCK/TYPE and independent fsck.fat -n. Not hardware or power-loss certification.\n'
 (ROOT/'build/dev11-fat32-large-report.txt').write_text(report+r.stdout+s.stdout+s.stderr);print(report)
finally:p.unlink()
