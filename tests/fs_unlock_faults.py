#!/usr/bin/env python3
"""Unlock must fail closed on read/flush errors. Never writes input images."""
from pathlib import Path
import sys,subprocess,re,hashlib
r=Path(__file__).resolve().parents[1];assert len(sys.argv)==2
for fs in ('fat32','exfat','ntfs'):
 found=list(Path(sys.argv[1]).glob('*-usb-'+fs+'.img'));assert len(found)==1
 image=found[0];before=hashlib.sha256(image.read_bytes()).digest()
 def run(side,cut):
  p=subprocess.run([str(r/'build/fs-unlock-probe'),str(image),str(side),str(cut)],capture_output=True,text=True,timeout=120)
  assert not p.returncode,(fs,side,cut,p.stdout,p.stderr)
  return p.stdout
 baseline=run(0,0);m=re.search(r'reads=(\d+) flushes=(\d+)',baseline);assert m
 cases=0
 for side,total in enumerate(map(int,m.groups())):
  points=sorted({1,total//2,total,*range(1,min(total,6)+1)}-{0}) if total else []
  for cut in points:run(side,cut);cases+=1
 assert hashlib.sha256(image.read_bytes()).digest()==before
 print(f'PASS {fs}: {cases} unlock read/flush failure points; no writes, write access denied, input SHA256 unchanged.',flush=True)
