#!/usr/bin/env python3
# MultiPulti | vm2 studios | t.me/v2vproj — malformed script input is never silently executed.
from pathlib import Path
import subprocess,struct,sys,tempfile,shutil
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from mkimage import entry
checks=0;log=[]
def check(x,msg):
 global checks
 checks+=1;assert x,msg
with tempfile.TemporaryDirectory() as td:
 td=Path(td)
 def run(payload):
  image=td/'fixture.img';shutil.copyfile(ROOT/'build/multios.img',image)
  with image.open('r+b') as f:
   base=2048*512;f.seek(base);boot=f.read(512);heap=struct.unpack_from('<I',boot,88)[0];spc=1<<boot[109];root=struct.unpack_from('<I',boot,96)[0]
   rootpos=base+(heap+(root-2)*spc)*512;f.seek(rootpos+96);old=f.read(96);first=struct.unpack_from('<I',old,52)[0]
   # Single-cluster valid fixture; reported size >4096 is an explicit rejection test only.
   f.seek(rootpos+96);f.write(entry('SCRIPT.TXT',first,len(payload)))
   f.seek(base+(heap+(first-2)*spc)*512);f.write(payload[:4096])
  p=subprocess.run([str(ROOT/'build/multios-host'),str(image)],input='source SCRIPT.TXT\nexit\n',text=True,capture_output=True,timeout=20)
  check(p.returncode==0,p.stdout+p.stderr);log.append(p.stdout);return p.stdout
 s=run(b'  # comment\r\n;comment\n\r\necho CRLF\r\necho final');check('\nCRLF\nfinal\n' in s and 'Error:' not in s,s)
 s=run(b'echo early\n\x00\necho forbidden');check('\nearly\n' not in s and '\nforbidden\n' not in s and 'Error:' in s,s)
 s=run(b'echo early\n\xff\n');check('\nearly\n' not in s and 'Error:' in s,s)
 s=run(b'echo "unterminated\necho forbidden');check('SOURCE stopped at line 1' in s and '\nforbidden\n' not in s,s)
 s=run(b'echo '+b'x'*506);check('\n'+('x'*506)+'\n' in s and 'Error:' not in s,s)
 s=run(b'echo '+b'x'*507);check('SOURCE stopped at line 1' in s and 'Error:' in s,s)
 s=run(b'#'+b'x'*4096);check('Error:' in s,s)
 s=run(b'');check('Error:' not in s,s)
(ROOT/'build/script-edges.log').write_text('\n'.join(log))
(ROOT/'build/script-edges-report.txt').write_text(f'{checks} SOURCE edge assertions passed: CRLF/comments/EOF, whole-input NUL/non-ASCII rejection, bad quotes, 511/512 line boundary, 4097 file-size rejection, empty script. Oversize metadata fixture is for pre-read rejection, not fsck certification.\n')
print((ROOT/'build/script-edges-report.txt').read_text())
