#!/usr/bin/env python3
"""Native UHCI keyboard + EHCI BOT system disk, BIOS boot without ATA.
No serial/PS2 input injection. IMAGE must use bios_disk=usb-bot, bios_input=auto.
"""
from pathlib import Path
import subprocess,socket,json,select,time,os,shutil,sys
ROOT=Path(__file__).resolve().parents[1]
def run(image,qemu):
 w=ROOT/'build'/'usb27-combo';w.mkdir(parents=True,exist_ok=True);disk=w/'disk.img';shutil.copyfile(image,disk)
 sock=w/'qmp.sock';sock.unlink(missing_ok=True)
 args=[qemu,'-machine','pc','-cpu','486','-m','64M','-drive',f'file={disk},format=raw,if=none,id=stick','-device','usb-ehci,id=store','-device','usb-storage,drive=stick,bus=store.0,bootindex=1','-device','piix3-usb-uhci,id=input','-device','usb-kbd,bus=input.0,usb_version=1','-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot']
 p=subprocess.Popen(args,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);data=bytearray();q=None
 def receive(marker,start=0):
  end=time.monotonic()+120
  while marker not in data[start:]:
   if time.monotonic()>end:raise TimeoutError(bytes(data[-2000:]))
   if select.select([p.stdout],[],[],.1)[0]:
    b=os.read(p.stdout.fileno(),65536)
    if not b:raise RuntimeError(bytes(data[-2000:]))
    data.extend(b)
 try:
  receive(b'[RO]> ');q=socket.socket(socket.AF_UNIX);q.settimeout(10);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
  def qmp(cmd,args={}):
   f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
   while True:
    r=json.loads(f.readline())
    if 'return' in r:return
    if 'error' in r:raise RuntimeError(r)
  qmp('qmp_capabilities')
  def command(cmd):
   start=len(data)
   for ch in cmd+'\n':
    code={' ':'spc','.':'dot','"':'apostrophe','\n':'ret'}.get(ch,ch.lower());keys=([{'type':'qcode','data':'shift'}] if ch.isupper() or ch=='"' else [])+[{'type':'qcode','data':code}]
    qmp('send-key',{'keys':keys,'hold-time':45});time.sleep(.13)
   if cmd!='exit':receive(b'> ',start)
   text=bytes(data[start:]).decode(errors='replace').replace('\r','');assert 'Error:' not in text,text
   return text
  assert 'pc-usb-bot' in command('modules')
  inv=command('usb');assert 'UHCI' in inv and 'keyboards=1' in inv and 'EHCI' in inv and 'USB BOT disk ready / RW capable' in inv,inv
  command('unlock YES');command('write COMBO.TXT "USB input disk"');assert '\nUSB input disk\n' in command('type COMBO.TXT')
  command('sync');command('lock');command('exit');tail,_=p.communicate(timeout=30);data.extend(tail);assert p.returncode==33,p.returncode
  print('PASS native UHCI keyboard + EHCI BOT disk, no IDE, HID WRITE/TYPE/SYNC, clean EXIT.',flush=True)
 finally:
  if q:q.close()
  if p.poll() is None:p.kill();p.wait()
  (w/'console.log').write_bytes(data);disk.unlink()
if __name__=='__main__':run(sys.argv[1],sys.argv[2])
