#!/usr/bin/env python3
"""Forced RO and mismatched boot identity. Only ordinary disposable image files."""
from pathlib import Path
import subprocess,sys,os,time,select,socket,json,shutil,hashlib
ROOT=Path(__file__).resolve().parents[1]
def run(source,qemu):
 for case in ['readonly','wrong-id']:
  w=ROOT/'build'/('usb27-guard-'+case);w.mkdir(parents=True,exist_ok=True);disk=w/'disk.img';boot=w/'boot.img'
  if case=='readonly':
   ini=w/'settings.ini';ini.write_text('osk=off\nbios_input=auto\nstorage=readonly\nbios_disk=usb-bot\n');disk.unlink(missing_ok=True)
   subprocess.run([sys.executable,str(ROOT/'tools/customize.py'),str(source),'--settings',str(ini),'--output',str(disk)],check=True,stdout=subprocess.DEVNULL)
  else:
   shutil.copyfile(source,disk);shutil.copyfile(source,boot)
   with disk.open('r+b') as f:f.seek(440);x=f.read(1);f.seek(440);f.write(bytes([x[0]^1]))
  before=hashlib.sha256(disk.read_bytes()).digest();sock=w/'qmp.sock';sock.unlink(missing_ok=True)
  args=[qemu,'-machine','pc','-cpu','486','-m','64M',*(['-drive',f'file={boot},format=raw,if=ide,index=0'] if case=='wrong-id' else []),'-drive',f'file={disk},format=raw,if=none,id=stick','-device','usb-ehci,id=usb','-device','usb-storage,drive=stick,bus=usb.0'+(',bootindex=1' if case=='readonly' else ''),'-display','none','-serial','stdio','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-no-reboot']
  p=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);data=bytearray();q=None
  def receive(marker,start=0):
   end=time.monotonic()+60
   while marker not in data[start:]:
    if time.monotonic()>end:raise TimeoutError(bytes(data[-2000:]))
    if select.select([p.stdout],[],[],.1)[0]:
     b=os.read(p.stdout.fileno(),65536)
     if not b:raise RuntimeError(bytes(data[-2000:]))
     data.extend(b)
  try:
   end=time.monotonic()+60
   while not sock.exists():
    if time.monotonic()>end:raise TimeoutError('QMP socket')
    time.sleep(.05)
   q=socket.socket(socket.AF_UNIX);q.settimeout(10);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
   def qmp(cmd,args={}):
    f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
    while True:
     r=json.loads(f.readline())
     if 'return' in r:return r['return']
     if 'error' in r:raise RuntimeError(r)
   qmp('qmp_capabilities')
   if case=='readonly':
    receive(b'[RO]> ')
    for cmd in ['modules','unlock YES','write DENIED.TXT "no"']:
     start=len(data);p.stdin.write((cmd+'\n').encode());p.stdin.flush();receive(b'> ',start)
     if cmd=='modules':assert b'block: pc-usb-bot-ro' in data[start:],data
     else:assert b'Error:' in data[start:],data
   else:
    capture=w/'vga.bin'
    while True:
     qmp('human-monitor-command',{'command-line':f'pmemsave 0xb8000 4000 "{capture}"'})
     text=capture.read_bytes()[::2].decode(errors='replace')
     if 'module boot failed' in text:break
     if time.monotonic()>end:raise TimeoutError(text)
     time.sleep(.1)
    (w/'vga.txt').write_text(text)
   qmp('quit');tail,_=p.communicate(timeout=15);data.extend(tail)
   assert hashlib.sha256(disk.read_bytes()).digest()==before,'Disk modified by refused operations'
   if case=='wrong-id':assert hashlib.sha256(boot.read_bytes()).digest()==hashlib.sha256(Path(source).read_bytes()).digest()
   print(case+': PASS; refusal; images unchanged.',flush=True)
  finally:
   if q:q.close()
   if p.poll() is None:p.kill();p.wait()
   (w/'console.log').write_bytes(data);disk.unlink(missing_ok=True);boot.unlink(missing_ok=True)
if __name__=='__main__':run(sys.argv[1],sys.argv[2])
