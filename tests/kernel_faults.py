#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — emulator fault injection, NOT hardware certification.
from pathlib import Path
import os,subprocess,select,time,hashlib,shutil,socket,json
ROOT=Path(__file__).resolve().parents[1]
machine=os.environ.get('MULTIOS_TEST_MACHINE','pc')
qemu=os.environ.get('QEMU') or shutil.which('qemu-system-x86_64')
if not qemu:raise RuntimeError('QEMU missing')
work=ROOT/'build/kernel-fault-tests';work.mkdir(exist_ok=True)
report=['MultiOS 0.12-dev UNPUBLISHED | MultiPulti | vm2 studios | t.me/v2vproj',
        'Machine='+machine+'; real CPU exceptions in QEMU TCG; isolated TEST kernels, no production fault command.']
cases=[(1,'Invalid opcode','VECTOR=0x00000006'),(2,'Divide error','VECTOR=0x00000000'),
       (3,'General protection','VECTOR=0x0000000D'),(4,'Double fault - emergency TSS','VECTOR=0x00000008'),
       (5,'Unexpected interrupt','VECTOR=0x00000081'),(6,'STACK CANARY FAILURE','compiler-detected stack corruption')]
for cpu in ('486','qemu64'):
 for number,name,marker in cases:
  image=ROOT/f'build/fault-{number}.img';before=hashlib.sha256(image.read_bytes()).digest()
  sock=work/f'{cpu}-{number}.sock'
  if sock.exists():sock.unlink()
  args=[qemu,'-machine',machine,'-accel','tcg','-cpu',cpu,'-m','32M','-drive',f'file={image},format=raw,if=ide,index=0',
        '-display','none','-monitor','none','-serial','stdio','-no-reboot',
        '-qmp',f'unix:{sock},server=on,wait=off']
  p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
  data=bytearray();deadline=time.monotonic()+25
  try:
   while b'HALTED - preserve the diagnostic' not in data:
    if time.monotonic()>deadline:raise TimeoutError(data.decode(errors='replace'))
    if select.select([p.stdout],[],[],.1)[0]:
     b=os.read(p.stdout.fileno(),8192)
     if not b:raise RuntimeError('Guest stopped/reset before diagnostic: '+data.decode(errors='replace'))
     data.extend(b)
   log=data.decode(errors='replace');assert name in log and marker in log,log
   if number==3:assert 'ERROR=0x0000FFF8' in log,log
   if number==4:assert 'independent 8 KiB stack' in log,log
   time.sleep(.1);assert p.poll() is None,'panic rebooted or exited'
   # Verify actual halt, IF=0 and emergency stack, not merely a printed message.
   with socket.socket(socket.AF_UNIX) as q:
    q.settimeout(5);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
    def cmd(name,args=None):
     f.write((json.dumps({'execute':name,'arguments':args or {}})+'\n').encode())
     while True:
      reply=json.loads(f.readline())
      if 'return' in reply:return reply['return']
      if 'error' in reply:raise RuntimeError(reply)
    cmd('qmp_capabilities')
    regs=cmd('human-monitor-command',{'command-line':'info registers'})
    assert 'HLT=1' in regs,regs
    import re
    efl=int(re.search(r'EFL=([0-9a-fA-F]+)',regs).group(1),16);assert not efl&0x200,regs
    if number==4:
     assert re.search(r'TR\s*=0020',regs),regs
     symbols=subprocess.check_output(['nm','-n',str(ROOT/f'build/fault-{number}.elf')],text=True)
     stack=next(int(line.split()[0],16) for line in symbols.splitlines() if line.split()[-1]=='df_stack')
     esp=int(re.search(r'ESP=([0-9a-fA-F]+)',regs).group(1),16)
     assert stack<=esp<stack+8192,(hex(esp),hex(stack))
    (work/f'{cpu}-{number}-registers.txt').write_text(regs)
    cmd('quit')
   p.wait(timeout=5)
   assert hashlib.sha256(image.read_bytes()).digest()==before,'fault path changed disk'
   (work/f'{cpu}-{number}.log').write_text(log)
   line=f'PASS {cpu}: {name}; CPU halted with IF=0, image unchanged.'
   report.append(line);print(line,flush=True)
  finally:
   if p.poll() is None:p.kill();p.wait()
report.append('12 injections passed. Page fault, NMI, machine check, physical hardware and arbitrary corrupt-stack recovery NOT certified.')
(ROOT/'build/kernel-fault-report.txt').write_text('\n'.join(report)+'\n')
print(report[-1])
