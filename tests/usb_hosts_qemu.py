#!/usr/bin/env python3
"""Native UHCI/OHCI/EHCI boot keyboards. No serial or PS/2 input injection.
Requires QEMU with classic USB hosts, USB-only extended BIOS image.
Usage: python tests/usb_hosts_qemu.py IMAGE [QEMU] [CASE...]
"""
from pathlib import Path
import subprocess,socket,json,select,time,os,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
CASES={
 'uhci':['-device','piix3-usb-uhci,id=usb','-device','usb-kbd,id=kbd,bus=usb.0,usb_version=1'],
 'ohci':['-device','pci-ohci,id=usb','-device','usb-kbd,id=kbd,bus=usb.0,usb_version=1'],
 'ehci':['-device','usb-ehci,id=usb','-device','usb-kbd,id=kbd,bus=usb.0,usb_version=2'],
 'ohci-port2':['-device','pci-ohci,id=usb','-device','usb-mouse,bus=usb.0,port=1,usb_version=1','-device','usb-kbd,id=kbd,bus=usb.0,port=2,usb_version=1'],
 'ehci-port2':['-device','usb-ehci,id=usb','-device','usb-mouse,bus=usb.0,port=1,usb_version=2','-device','usb-kbd,id=kbd,bus=usb.0,port=2,usb_version=2'],
 'companion-ohci':['-device','ich9-usb-ehci1,id=ehci','-device','pci-ohci,id=usb,masterbus=ehci.0,firstport=0','-device','usb-kbd,id=kbd,bus=ehci.0,port=1,usb_version=1'],
 'companion':['-device','ich9-usb-ehci1,id=ehci','-device','ich9-usb-uhci1,id=usb,masterbus=ehci.0,firstport=0','-device','usb-kbd,id=kbd,bus=ehci.0,port=1,usb_version=1'],
}
def run(label,image,qemu):
 w=ROOT/'build'/('usb26-'+label);w.mkdir(parents=True,exist_ok=True);img=w/'boot.img';shutil.copyfile(image,img)
 sock=w/'qmp.sock';sock.unlink(missing_ok=True)
 args=[qemu,'-machine','pc','-accel','tcg','-cpu','486','-m','64M','-drive',f'file={img},format=raw,if=ide,index=0',*CASES[label],'-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot']
 p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);data=bytearray();q=None
 def drain():
  while select.select([p.stdout],[],[],0)[0]:
   b=os.read(p.stdout.fileno(),65536)
   if not b:break
   data.extend(b)
 def receive(marker,start=0,timeout=40):
  end=time.monotonic()+timeout
  while marker not in data[start:]:
   if time.monotonic()>end:raise TimeoutError(bytes(data[-2500:]).decode(errors='replace'))
   if select.select([p.stdout],[],[],.1)[0]:
    b=os.read(p.stdout.fileno(),65536)
    if not b:raise RuntimeError(bytes(data).decode(errors='replace'))
    data.extend(b)
 try:
  receive(b'[RO]> ');q=socket.socket(socket.AF_UNIX);q.settimeout(10);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
  def qmp(cmd,args=None):
   f.write((json.dumps({'execute':cmd,'arguments':args or {}})+'\n').encode())
   while True:
    a=json.loads(f.readline())
    if 'return' in a:return a['return']
    if 'error' in a:raise RuntimeError(a)
  qmp('qmp_capabilities')
  def key(ch,shift=False):
   keys=([{'type':'qcode','data':'shift'}] if shift else [])+[{'type':'qcode','data':ch}]
   qmp('send-key',{'keys':keys,'hold-time':45});time.sleep(.13);drain()
  def command(cmd):
   drain();start=len(data)
   for ch in cmd:
    key({' ':'spc','/':'slash','.':'dot','"':'apostrophe','!':'1','-':'minus','_':'minus'}.get(ch,ch.lower()),ch.isupper() or ch in '"!_')
   key('ret')
   if cmd!='exit':receive(b'> ',start)
   return bytes(data[start:]).decode(errors='replace').replace('\r','')
  log=command('usb');expected='OHCI' if label.startswith('ohci') or label=='companion-ohci' else 'EHCI' if label.startswith('ehci') else 'UHCI'
  assert any(expected in line and 'keyboards=1' in line and 'boot-keyboard active' in line for line in log.splitlines()),log
  assert '\nUSB native\n' in command('echo USB native')
  # Each keyboard uses the shared HID/editor path, including Shift and Delete.
  drain();start=len(data)
  for ch in 'echo abx':key('spc' if ch==' ' else ch)
  key('left');key('delete');key('c');key('ret');receive(b'> ',start)
  assert b'\r\nabc\r\n' in data[start:],bytes(data[start:])
  command('unlock YES');command('write USB26.TXT "USB native file"')
  assert '\nUSB native file\n' in command('type USB26.TXT')
  command('sync');command('lock')
  # Exercise keyboard unplug/replug through real host hardware emulation.
  qmp('device_del',{'id':'kbd'});time.sleep(1);drain()
  props={'driver':'usb-kbd','id':'kbd2','bus':'ehci.0' if label.startswith('companion') else 'usb.0','port':'2' if label.endswith('port2') else '1','usb_version':2 if label.startswith('ehci') else 1}
  qmp('device_add',props)
  # Wait for guest enumeration, not a guessed host-side two-second sleep.
  symbols=subprocess.check_output(['nm','-n',os.environ.get('MULTIOS_USB_ELF',str(ROOT/'build/bios-rw/kernel.elf'))],text=True)
  address=next(int(line.split()[0],16) for line in symbols.splitlines() if line.split()[-1]=='usb')
  statefile=w/'inventory.bin';deadline=time.monotonic()+12
  while True:
   qmp('human-monitor-command',{'command-line':f'pmemsave {address} 336 "{statefile}"'})
   state=statefile.read_bytes()
   import struct
   if any(state[o+3]==1 and struct.unpack_from('<I',state,o+12)[0]==1 for o in range(0,336,28)):break
   if time.monotonic()>deadline:raise TimeoutError('Guest did not finish USB reconnect enumeration')
   time.sleep(.1)
  time.sleep(.2)
  assert '\nRECONNECTED\n' in command('echo RECONNECTED')
  log=command('usb');assert any(expected in line and 'disconnects=1' in line and 'keyboards=1' in line for line in log.splitlines()),log
  command('exit');tail,_=p.communicate(timeout=15);data.extend(tail);assert p.returncode==33,p.returncode
  print(label+': PASS; native USB-only boot HID, Shift, editor, filesystem WRITE/TYPE/SYNC, unplug/replug, clean EXIT.',flush=True)
 finally:
  if q:q.close()
  if p.poll() is None:p.kill();p.wait()
  (w/'console.log').write_bytes(data)
  img.unlink(missing_ok=True)
if __name__=='__main__':
 image=Path(sys.argv[1]);qemu=sys.argv[2] if len(sys.argv)>2 else 'qemu-system-i386'
 for c in sys.argv[3:] or CASES:run(c,image,qemu)
