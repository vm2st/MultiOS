#!/usr/bin/env python3
# MultiPulti | vm2 studios | t.me/v2vproj — native BIOS UHCI, NO PS/2 keyboard.
from pathlib import Path
import subprocess,socket,json,select,time,os,shutil,re
ROOT=Path(__file__).resolve().parents[1]
def run(label,controller='piix3-usb-uhci',mouse=False,usb_only=True):
 w=ROOT/'build'/('native-usb-'+label);w.mkdir(exist_ok=True)
 img=w/'boot.img';shutil.copyfile(ROOT/('build/usb-only-proof.img' if usb_only else 'build/multios.img'),img)
 sock=w/'qmp.sock'
 if sock.exists():sock.unlink()
 args=[os.environ.get('QEMU','qemu-system-x86_64'),'-machine','pc','-accel','tcg','-cpu','486','-m','32M','-drive',f'file={img},format=raw,if=ide,index=0','-device',controller+',id=uhci','-device','usb-kbd,id=kbd,bus=uhci.0,port='+('2' if mouse else '1'),'-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot']
 if mouse:args.extend(['-device','usb-mouse,bus=uhci.0,port=1'])
 p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 data=bytearray();q=None
 def drain():
  while select.select([p.stdout],[],[],0)[0]:
   b=os.read(p.stdout.fileno(),65536)
   if not b:break
   data.extend(b)
 def receive(marker,start=0,timeout=30):
  end=time.monotonic()+timeout
  while marker not in data[start:]:
   if time.monotonic()>end:raise TimeoutError(bytes(data[-2000:]).decode(errors='replace'))
   ready,_,_=select.select([p.stdout],[],[],.1)
   if ready:
    b=os.read(p.stdout.fileno(),65536)
    if not b:raise RuntimeError(bytes(data).decode(errors='replace'))
    data.extend(b)
 try:
  receive(b'[RO]> ')
  q=socket.socket(socket.AF_UNIX);q.settimeout(10);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
  def qmp(name,arguments=None):
   f.write((json.dumps({'execute':name,'arguments':arguments or {}})+'\n').encode())
   while True:
    a=json.loads(f.readline())
    if 'return' in a:return a['return']
    if 'error' in a:raise RuntimeError(a)
  qmp('qmp_capabilities')
  def key(k,shift=False):
   keys=([{'type':'qcode','data':'shift'}] if shift else [])+[{'type':'qcode','data':k}]
   qmp('send-key',{'keys':keys,'hold-time':35});time.sleep(.09);drain()
  def command(cmd):
   drain();start=len(data)
   for ch in cmd:
    names={' ':'spc','/':'slash','.':'dot','"':'apostrophe',':':'semicolon','!':'1','-':'minus','_':'minus','\\':'backslash'}
    key(names.get(ch,ch.lower()),ch.isupper() or ch in '\":!_')
   key('ret')
   if cmd!='exit':receive(b'> ',start)
   return bytes(data[start:]).decode(errors='replace').replace('\r','')
  log=command('usb');assert 'boot-keyboard active' in log and 'keyboards=1' in log,log
  assert re.search(r'reports=[1-9]',log),log
  log=command('echo USB native');assert '\nUSB native\n' in log,log
  if label=='piix3':
   drain();start=len(data)
   for ch in 'echo abx':key('spc' if ch==' ' else ch)
   key('left');key('delete');key('c');key('ret');receive(b'> ',start)
   assert b'\r\nabc\r\n' in data[start:],bytes(data[start:])
   start=len(data);key('f8');key('ret');receive(b'> ',start)
   assert b'\r\nabc\r\n' in data[start:],bytes(data[start:])
   # Native USB F6/F7/optional F5, not firmware PS/2 emulation.
   start=len(data);key('f6');key('f7');key('f5');key('f7');receive(b'> ',start)
   assert b'^C' in data[start:],bytes(data[start:]);key('f2')
  log=command('devices');assert 'vendor=8086' in log and 'class=0C/03/00' in log,log
  command('unlock YES');command('write USB.TXT "USB keyboard file"')
  log=command('type USB.TXT');assert '\nUSB keyboard file\n' in log,log
  if usb_only:
   command('exit');tail,_=p.communicate(timeout=20);data.extend(tail);assert p.returncode==33,p.returncode
  else:
   command('sync');qmp('device_del',{'id':'kbd'});time.sleep(.8);drain()
   # Confirm the native driver's live inventory via RAM, no fallback input.
   symbols=subprocess.check_output(['nm','-n',str(ROOT/'build/kernel.elf')],text=True)
   address=next(int(line.split()[0],16) for line in symbols.splitlines() if line.split()[-1]=='usb')
   saved=w/'inventory.bin'
   qmp('human-monitor-command',{'command-line':f'pmemsave {address} 20 "{saved}"'})
   import struct
   state=saved.read_bytes();assert state[3]==0 and struct.unpack_from('<I',state,12)[0]==1,state.hex()
   qmp('quit');tail,_=p.communicate(timeout=20);data.extend(tail);assert p.returncode==0
  assert b'Error:' not in data,bytes(data[-2000:])
  return f'{label}: PASS; USB-only input profile={usb_only}, no serial input injection; native {controller} boot keyboard, shift, PCI, exFAT WRITE/TYPE; '+('clean OS EXIT.' if usb_only else 'native unplug detection verified; QMP quit after SYNC.')

 finally:
  (w/'console.log').write_bytes(data)
  if q:q.close()
  if p.poll() is None:p.kill();p.wait()
if __name__=='__main__':
 lines=[]
 for label,kind,mouse in [('piix3','piix3-usb-uhci',False),('piix4-port2','piix4-usb-uhci',True),('default-auto','piix3-usb-uhci',False)]:
  s=run(label,kind,mouse,label!='default-auto');print(s,flush=True);lines.append(s)
 (ROOT/'build/native-usb-report.txt').write_text('\n'.join(lines)+'\nEmulation only; no physical USB hardware certification.\n')
