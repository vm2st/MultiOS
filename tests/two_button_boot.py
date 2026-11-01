#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — BIOS + UEFI, ONLY F6/F7 injected into keyboard.
from pathlib import Path
import json,os,re,select,shutil,socket,subprocess,time
ROOT=Path(__file__).resolve().parents[1]
NORMAL=['1234567890-=','qwertyuiop[]',"asdfghjkl;'`",'zxcvbnm,./\\~']
UPPER=['!@#$%^&*()_+','QWERTYUIOP{}','ASDFGHJKL:"~','ZXCVBNM<>?|`']
def run(mode, previous=False):
 w=ROOT/'build'/(('three-button-' if previous else 'two-button-')+mode);w.mkdir(exist_ok=True)
 image=w/'boot.img';shutil.copyfile(ROOT/'build/MultiOS-MultiPulti.img',image)
 sock=w/'qmp.sock'
 if sock.exists():sock.unlink()
 common=['-accel','tcg','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-no-reboot']
 if mode=='bios':
  args=[os.environ.get('QEMU','qemu-system-x86_64'),'-machine','pc','-cpu','486','-m','32M','-drive',f'file={image},format=raw,if=ide,index=0','-serial','stdio','-device','isa-debug-exit,iobase=0xf4,iosize=0x04',*common]
 else:
  var=w/'vars.fd';shutil.copyfile('/usr/share/OVMF/OVMF_VARS.fd',var)
  args=[os.environ.get('QEMU_UEFI','/usr/bin/qemu-system-x86_64'),'-machine','q35','-m','256M','-drive','if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE.fd','-drive',f'if=pflash,format=raw,file={var}','-device','qemu-xhci,id=xhci','-drive',f'if=none,format=raw,id=stick,file={image}','-device','usb-storage,drive=stick,bus=xhci.0,bootindex=1','-device','usb-kbd,bus=xhci.0','-serial','null','-serial','stdio',*common]
 p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,stdin=subprocess.DEVNULL)
 data=bytearray();q=None;events=0
 def clean():return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',bytes(data)).decode(errors='replace').replace('\r','')
 def receive(marker,start=0):
  end=time.monotonic()+60
  while marker not in clean()[start:]:
   if time.monotonic()>end:raise TimeoutError(clean()[-3000:])
   ready,_,_=select.select([p.stdout],[],[],.1)
   if ready:
    b=os.read(p.stdout.fileno(),65536)
    if not b:raise RuntimeError(clean()[-3000:])
    data.extend(b)
 def drain():
  while select.select([p.stdout],[],[],0)[0]:
   b=os.read(p.stdout.fileno(),65536)
   if not b:break
   data.extend(b)
 try:
  receive('[RO]> ')
  q=socket.socket(socket.AF_UNIX);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
  def qmp(name,arguments=None):
   f.write((json.dumps({'execute':name,'arguments':arguments or {}})+'\n').encode())
   while True:
    a=json.loads(f.readline())
    if 'return' in a:return a['return']
    if 'error' in a:raise RuntimeError(a)
  qmp('qmp_capabilities')
  def key(k):
   nonlocal events
   assert k in (('f5','f6','f7') if previous else ('f6','f7'));events+=1
   qmp('send-key',{'keys':[{'type':'qcode','data':k}],'hold-time':40})
   time.sleep(.15 if mode=='uefi' else .065);drain()
  position=0;shift=0;enabled=mode=='uefi'
  def choose(target):
   nonlocal position
   while position!=target:
    backward=previous and (position-target)%56 < (target-position)%56
    key('f5' if backward else 'f6');position=(position+(-1 if backward else 1))%56
   key('f7')
  def type_char(ch):
   nonlocal enabled,shift
   if not enabled:key('f7');enabled=True
   if ch=='\n':choose(50);return
   if ch==' ':choose(48);return
   found=None
   for sh,rows in enumerate((NORMAL,UPPER)):
    for r,row in enumerate(rows):
     if ch in row:found=(sh,r*12+row.index(ch));break
    if found:break
   assert found,ch
   sh,target=found
   if shift!=sh:choose(51);shift=sh
   choose(target)
  for cmd in ('ver','osk off','echo ok', 'exit' if mode=='bios' else 'poweroff'):
   drain();start=len(clean())
   for ch in cmd+'\n':type_char(ch)
   if cmd in ('exit','poweroff'):break
   receive('> ',start)
   if cmd=='ver':assert 'UNPUBLISHED DEVELOPMENT BUILD' in clean()[start:]
   if cmd=='osk off':enabled=False
   if cmd=='echo ok':
    # Firmware text uses cursor commands rather than CR/LF in serial capture.
    text=clean()[start:]
    assert (re.search(r'ok\s*MultiOS:/',text) if mode=='uefi' else '\nok\n' in text), text[-1000:]
  tail,_=p.communicate(timeout=30);data.extend(tail)
  assert p.returncode==(33 if mode=='bios' else 0),clean()[-2000:]
  assert 'Error:' not in clean(),clean()[-2000:]
  (w/'console.log').write_text(clean())
  return f'{mode} ({3 if previous else 2} buttons): PASS, {events} physical key events; optional previous={previous}; VER, OSK OFF, reopen, ECHO, stop. No serial/ASCII/arrow input.'
 finally:
  (w/'console-raw.log').write_text(clean())
  if q:q.close()
  if p.poll() is None:p.kill();p.wait()
lines=[]
for mode,previous in [('bios',False),('uefi',False),('bios',True),('uefi',True)]:
 line=run(mode,previous);lines.append(line);print(line,flush=True)
(ROOT/'build/two-button-boot-report.txt').write_text('\n'.join(lines)+'\nQEMU emulation, not physical two-button hardware certification.\n')
print('\n'.join(lines))
