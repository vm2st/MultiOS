#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — synchronous flush error, not real power loss.
from pathlib import Path
import subprocess,tempfile,shutil,os,hashlib
ROOT=Path(__file__).resolve().parents[1]
binary=ROOT/'build/multios-flush-fault-host'
lib=['lib/util.c','lib/fs.c','lib/exfat.c','lib/fat32.c','lib/ntfs.c','lib/ntfs_meta.c','lib/ntfs_attr.c','lib/block64.c','lib/shell.c','core/hal.c','core/osk.c','core/modules.c','core/cpu.c','core/hardware.c','core/hid.c','core/editor.c']
subprocess.run(['gcc','-std=c11','-O1','-Ilib','-Iinclude','-I.','-DMULTIOS_FAULT_INJECTION','platform/posix/host.c',*lib,'-o',str(binary)],cwd=ROOT,check=True)
lines=[];checks=0
with tempfile.TemporaryDirectory() as td:
 td=Path(td)
 for cut in range(4):
  results=[]
  for extra in (False,True):
   img=td/f'{cut}-{extra}.img';shutil.copyfile(ROOT/'build/multios.img',img)
   env=dict(os.environ,MULTIOS_FAIL_FLUSH_AT=str(cut))
   commands='unlock YES\nwrite FIRST.TXT "first"\n'+('touch AFTER.TXT\n' if extra else '')+'exit\n'
   p=subprocess.run([str(binary),str(img),'--allow-write'],input=commands,text=True,capture_output=True,env=env,timeout=20)
   assert p.returncode==1 and 'Error:' in p.stdout,p.stdout;checks+=1
   results.append(hashlib.sha256(img.read_bytes()).digest())
  assert results[0]==results[1],f'Write after failed flush {cut} changed volume';checks+=1
  lines.append(f'PASS WRITE flush point {cut}: poisoned/RO; extra TOUCH produces identical bytes.')
 for cmd in ('sync','lock'):
  img=td/(cmd+'.img');shutil.copyfile(ROOT/'build/multios.img',img);before=hashlib.sha256(img.read_bytes()).digest()
  p=subprocess.run([str(binary),str(img),'--allow-write'],input=f'unlock YES\n{cmd}\ntouch AFTER.TXT\nexit\n',text=True,capture_output=True,env=dict(os.environ,MULTIOS_FAIL_FLUSH_AT='0'),timeout=20)
  assert p.returncode==1 and 'Error:' in p.stdout and hashlib.sha256(img.read_bytes()).digest()==before,p.stdout;checks+=1
  lines.append('PASS '+cmd.upper()+' flush failure: subsequent TOUCH blocked; unchanged image.')
report=f'{checks} synchronous flush-failure assertions PASS. Four WRITE barriers plus SYNC/LOCK failure. No torn-sector, controller-cache or physical power-loss simulation.\n'
(ROOT/'build/dev10-flush-fault-report.txt').write_text(report+'\n'.join(lines)+'\n');print(report)
