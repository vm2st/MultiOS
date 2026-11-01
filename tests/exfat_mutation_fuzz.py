#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — reproducible shallow mutation campaign, not full fuzz coverage.
from pathlib import Path
import subprocess,tempfile,shutil,random,os,struct
ROOT=Path(__file__).resolve().parents[1]
exe=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-host'))
rng=random.Random(0x4d50554c);logs=[]
with tempfile.TemporaryDirectory() as td:
 image=Path(td)/'mutate.img';shutil.copyfile(ROOT/'build/multios.img',image)
 base=2048*512
 with image.open('rb') as f:
  f.seek(base);b=f.read(512);fat=struct.unpack_from('<I',b,80)[0];heap=struct.unpack_from('<I',b,88)[0];spc=1<<b[109]
 ranges=[(base,512),(base+11*512,512),(base+12*512,512),(base+23*512,512),
         (base+fat*512,128),(base+heap*512,1024),(base+(heap+8*spc)*512,2048)]
 for i in range(256):
  start,length=rng.choice(ranges);pos=start+rng.randrange(length)
  with image.open('r+b') as f:f.seek(pos);old=f.read(1);f.seek(pos);f.write(bytes([old[0]^rng.randrange(1,256)]))
  try:
   env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1')
   p=subprocess.run([exe,str(image),'--allow-write'],input='dir\nunlock YES\nexit\n',capture_output=True,text=True,timeout=10,env=env)
   assert p.returncode in (0,1),(i,pos,p.returncode,p.stdout,p.stderr)
   assert not any(x in p.stderr for x in ('Sanitizer','runtime error:')),(i,p.stderr)
   logs.append(f'PASS seed=4D50554C case={i} offset={pos} return={p.returncode}')
  finally:
   with image.open('r+b') as f:f.seek(pos);f.write(old)
report='256 seeded exFAT single-byte mutation runs PASS: boot/checksum/root/bitmap/FAT inputs, DIR/UNLOCK/EXIT; no sanitizer findings or timeouts. Read-only command sequence; not exhaustive fuzzing or a power-loss simulator.\n'
(ROOT/'build/dev10-mutation-report.txt').write_text(report)
(ROOT/'build/dev10-mutation-cases.txt').write_text('\n'.join(logs)+'\n');print(report)
