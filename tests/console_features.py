#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — host harness tests real shell/exFAT, not boot proof.
from pathlib import Path
import subprocess,tempfile,shutil,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from mkimage import create
checks=0;logs=[]
def check(value,message):
 global checks
 checks+=1
 assert value,message
with tempfile.TemporaryDirectory() as td:
 td=Path(td);image=td/'test.img';shutil.copyfile(ROOT/'build/multios.img',image)
 def run(text):
  p=subprocess.run([str(ROOT/'build/multios-host'),str(image),'--allow-write'],input=text,text=True,capture_output=True,timeout=30)
  logs.append(p.stdout);check(p.returncode==0,p.stderr+p.stdout);return p.stdout
 s=run('echo abx\x02\x04c\nexit\n');check('\nabc\n' in s,s)
 s=run('echo bc\x01\x06\x06\x06\x06\x06a\x05\nexit\n');check('\nabc\n' in s,s)
 s=run('echo history\n\x10\n\x10\x0e'+'echo draft\nexit\n');check(s.count('\nhistory\n')==2,s);check('\ndraft\n' in s,s)
 s=run('unlock YES\nwrite ok.txt "echo first"\nappend ok.txt "echo second"\nsource ok.txt\nexit\n');check('\nfirst\nsecond\n' in s,s);check('Error:' not in s,s)
 s=run('unlock YES\nwrite fail.txt "missingcommand"\nappend fail.txt "echo forbidden"\nsource fail.txt\nexit\n');check('SOURCE stopped at line 1' in s,s);check('\nforbidden\n' not in s,s)
 s=run('unlock YES\nwrite loop.txt "source loop.txt"\nsource loop.txt\nexit\n');check('Error:' in s and s.count('SOURCE stopped at line 1')==4,s)
 s=run('unlock YES\nwrite pages.txt "page01"\n'+''.join(f'append pages.txt "page{i:02}"\n' for i in range(2,31))+'exit\n');check('Error:' not in s,s)
 s=run('more pages.txt\n \n \nexit\n');check('-- MORE:' in s and 'page30' in s,s)
 s=run('more pages.txt\nq\nexit\n');check('page01' in s and 'page30' not in s,s)
 s=run('usb\ndevices\nexit\n');check('Native PCI scan unavailable' in s,s);check('No native UHCI controller active' in s,s)
 part=td/'data.exfat'
 with image.open('rb') as f,part.open('wb') as o:f.seek(2048*512);o.write(f.read())
 p=subprocess.run(['fsck.exfat','-n',str(part)],text=True,capture_output=True);check(p.returncode==0,p.stdout+p.stderr);logs.append(p.stdout)
(ROOT/'build/console-features.log').write_text('\n'.join(logs))
(ROOT/'build/console-features-report.txt').write_text(f'{checks} console integration assertions passed: insertion/deletion/home/end, history/draft, SOURCE success/failure/recursion, MORE next/quit, unavailable platform diagnostics, fsck.\n')
print((ROOT/'build/console-features-report.txt').read_text())
