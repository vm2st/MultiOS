#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — isolated host build header-dependency regression.
from pathlib import Path
import tempfile,shutil,subprocess,hashlib
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='multipulti-dependencies-') as td:
 root=Path(td)
 for name in ['Makefile','core','drivers','include','lib','platform','profiles','tools','tests']:
  p=ROOT/name
  if p.is_dir():shutil.copytree(p,root/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
  else:shutil.copy2(p,root/name)
 targets=['build/multios-host','build/two-button-shell']
 def make(*args):subprocess.run(['make',*args],cwd=root,check=True,stdout=subprocess.DEVNULL)
 make(*targets)
 before={n:hashlib.sha256((root/n).read_bytes()).hexdigest() for n in targets}
 header=root/'include/multios/brand.h';original=header.read_text()
 changed=original.replace('0.12-dev','0.12-dev-HEADER-DEPENDENCY')
 assert changed!=original;header.write_text(changed)
 for n in targets:
  assert subprocess.run(['make','-q',n],cwd=root).returncode==1,n
 make(*targets)
 for n in targets:assert b'0.12-dev-HEADER-DEPENDENCY' in (root/n).read_bytes(),n
 header.write_text(original);make(*targets)
 for n in targets:assert hashlib.sha256((root/n).read_bytes()).hexdigest()==before[n],n
report='PASS isolated build dependency regression: changing only brand.h invalidates and rebuilds host + two-button harness, new version is present, restoring the header reproduces original binaries byte-for-byte.\n'
(ROOT/'build/dev09-build-dependency-report.txt').write_text(report);print(report)
