#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — FAT32 synchronous write/flush failure injection.
from pathlib import Path
import shutil,subprocess,os,hashlib
ROOT=Path(__file__).resolve().parents[1];w=ROOT/'build/fs11-faults';w.mkdir(exist_ok=True)
source=w/'clean.img'
with source.open('wb') as f:f.truncate(64*1024*1024)
subprocess.run(['mkfs.fat','-F','32',str(source)],check=True,stdout=subprocess.DEVNULL)
# Same provider fault-injection build used by the established exFAT tests.
binary=ROOT/'build/multios-fault-host';lines=[]
for mode,limit in [('WRITE',20),('FLUSH',8)]:
 for cut in range(limit):
  hashes=[];failed=False
  for extra in (False,True):
   image=w/('with.img' if extra else 'without.img');shutil.copyfile(source,image)
   env=dict(os.environ);env['MULTIOS_FAIL_'+mode+'_AT']=str(cut)
   commands='unlock YES\nwrite crash.txt "first"\n'+('touch guard.txt\n' if extra else '')+'exit\n'
   p=subprocess.run([str(binary),str(image),'--allow-write'],input=commands,text=True,capture_output=True,env=env,timeout=60)
   assert p.returncode in (0,1),p.stdout+p.stderr
   if not extra:failed='Error:' in p.stdout.split('> exit',1)[0]
   if failed:hashes.append(hashlib.sha256(image.read_bytes()).digest())
  if failed:
   assert hashes[0]==hashes[1],(mode,cut,'post-error mutation was possible')
   lines.append(f'PASS {mode} first-operation failure {cut}: following TOUCH cannot alter bytes.')
for operation in ('sync','lock'):
 image=w/'flush-guard.img';shutil.copyfile(source,image);before=hashlib.sha256(image.read_bytes()).digest()
 p=subprocess.run([str(binary),str(image),'--allow-write'],input='unlock YES\n'+operation+'\nmount auto\ntouch guard.txt\nexit\n',text=True,capture_output=True,env=dict(os.environ,MULTIOS_FAIL_FLUSH_AT='0'),timeout=60)
 assert p.returncode==1 and p.stdout.count('Error:')==3,p.stdout+p.stderr
 assert hashlib.sha256(image.read_bytes()).digest()==before,operation
 lines.append('PASS '+operation.upper()+' failure: RO/poison persists across MOUNT AUTO and following TOUCH; byte-identical source.')
assert len(lines)>=14,len(lines)
report=f'{len(lines)} FAT32 synchronous first-operation fault positions PASS: after injected WRITE/FLUSH failure, subsequent TOUCH leaves exactly the same bytes as stopping after failure. No power-loss/torn-sector guarantee.\n'
(ROOT/'build/dev11-fat32-fault-report.txt').write_text(report+'\n'.join(lines)+'\n');print(report)
