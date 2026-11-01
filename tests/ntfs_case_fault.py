#!/usr/bin/env python3
"""Case-distinct NTFS write fault matrix; prepare ntfs_case.py fixtures first."""
from pathlib import Path
import os,subprocess,hashlib
r=Path(__file__).resolve().parents[1];image=r/'build/ntfs-case32/seed.img';original=hashlib.sha256(image.read_bytes()).digest()
for binary in ('ntfs-case-tx-test','ntfs-case-tx-sanitize'):
 p=subprocess.run([str(r/'build'/binary),str(image)],capture_output=True,timeout=3600,env=dict(os.environ,MULTIOS_NTFS_TEST_TIME='1'))
 text=(p.stdout+p.stderr).decode(errors='replace');(image.parent/(binary+'.log')).write_text(text)
 assert not p.returncode and 'PASS real-FS private transactions:' in text,(binary,p.returncode,text[-3000:])
 assert hashlib.sha256(image.read_bytes()).digest()==original
 print(binary+': '+text.splitlines()[-1],flush=True)
print('PASS NTFS exact-name ordinary/sanitizer fault matrices; original fixture unchanged. Model only.')
