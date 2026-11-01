#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — bounded, deterministic mutation smoke, NOT exhaustive.
from pathlib import Path
import os,subprocess,random,hashlib,struct
ROOT=Path(__file__).resolve().parents[1];w=ROOT/'build/fs11-tests';host=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-sanitize-host'))
rand=random.Random(0x4d503131);lines=[]
for kind in ('fat','ntfs'):
 original=(w/('clean-fat.img' if kind=='fat' else 'ntfs.img')).read_bytes()
 if kind=='fat':
  reserved=struct.unpack_from('<H',original,14)[0];fs=struct.unpack_from('<I',original,36)[0];heap=(reserved+original[16]*fs)*512
  regions=[(0,512),(512,512),(reserved*512,512),((reserved+fs)*512,512),(heap,512)]
 else:
  mft=struct.unpack_from('<Q',original,48)[0]*original[13]*512
  regions=[(0,512),(mft,1024),(mft+3*1024,1024),(mft+5*1024,1024),(mft+64*1024,1024)]
 p=w/f'mutation-{os.getpid()}.img'
 for j in range(80):
  b=bytearray(original);start,n=regions[j%len(regions)];off=start+rand.randrange(n);b[off]^=1<<rand.randrange(8);p.write_bytes(b)
  before=hashlib.sha256(b).digest()
  commands='dir\nfs\ncrc32 resident.txt\nunlock YES\nexit\n' if kind=='ntfs' else 'dir\nfs\ntype file.txt\nunlock YES\nexit\n'
  r=subprocess.run([host,str(p),'--allow-write'],input=commands,text=True,capture_output=True,timeout=30)
  assert r.returncode==0,(kind,j,r.stdout,r.stderr)
  assert 'AddressSanitizer' not in r.stderr and 'runtime error:' not in r.stderr,(kind,j,r.stderr)
  assert hashlib.sha256(p.read_bytes()).digest()==before,(kind,j,'read/unlock mutated bytes')
  lines.append(f'PASS {kind} mutation {j}: offset={off}')
 p.unlink()
report='160 deterministic FAT32/NTFS one-bit mutation runs PASS under ASan/UBSan: no sanitizer findings, process crashes, timeouts or source changes. Read/unlock only; not exhaustive and not a power-loss test.\n'
(ROOT/'build/dev11-filesystems-fuzz-report.txt').write_text(report)
(ROOT/'build/dev11-filesystems-fuzz-cases.txt').write_text('\n'.join(lines)+'\n');print(report)
