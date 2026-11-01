#!/usr/bin/env python3
"""Extended real-FS fault matrices for protected-source copies and case renames.
Prepare fixtures with fs_writeops.py IMAGES --prepare-only first.
Uses the shared bounded persistence model, not physical power-cut testing.
"""
from pathlib import Path
import subprocess,hashlib,os
env=dict(os.environ,MULTIOS_NTFS_TEST_TIME='1') # Deterministic timestamps for byte-exact old/new comparison.
root=Path(__file__).resolve().parents[1];work=root/'build/writeops31'
for fs in ('fat32','exfat','ntfs'):
 image=work/(fs+'.img');assert image.is_file() and not image.is_symlink();before=hashlib.sha256(image.read_bytes()).digest()
 for binary in ('writeops-tx-test','writeops-tx-sanitize'):
  p=subprocess.run([str(root/'build'/binary),str(image)],capture_output=True,timeout=1800,env=env)
  text=p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace');(work/(fs+'-'+binary+'.log')).write_text(text)
  assert not p.returncode and 'PASS real-FS private transactions:' in text,(fs,binary,p.returncode,text[-4000:])
  assert hashlib.sha256(image.read_bytes()).digest()==before
  print(fs+'/'+binary+': '+text.splitlines()[-1],flush=True)
print('PASS three FS x ordinary/sanitizer: protected-source copies, case-only file/directory renames, append/write-at/resize, crash/I/O/recovery matrices; source fixture images unchanged. Model only.')
