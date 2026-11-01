#!/usr/bin/env python3
"""EDD stage-2 refusal checks; corrupted ordinary copies, never real disks."""
from pathlib import Path
import sys,subprocess,time,socket,json,hashlib,struct,shutil
ROOT=Path(__file__).resolve().parents[1]
def run(source,qemu):
 for kind,message in [('size','BIOS EDD payload I/O or size error'),('crc','BIOS payload CRC mismatch'),('ram','need E820 usable RAM')]:
  w=ROOT/'build'/('usb27-loader-'+kind);w.mkdir(parents=True,exist_ok=True);image=w/'disk.img';shutil.copyfile(source,image)
  with image.open('r+b') as f:
   if kind=='size':f.seek(17*512+8);f.write(struct.pack('<I',0))
   elif kind=='crc':f.seek(18*512+100);x=f.read(1);f.seek(-1,1);f.write(bytes([x[0]^1]))
  before=hashlib.sha256(image.read_bytes()).digest();sock=w/'qmp.sock';sock.unlink(missing_ok=True)
  args=[qemu,'-machine','pc','-cpu','486','-m','16M' if kind=='ram' else '64M','-drive',f'file={image},format=raw,if=none,id=stick','-device','usb-ehci,id=usb','-device','usb-storage,drive=stick,bus=usb.0,bootindex=1','-display','none','-serial','none','-qmp',f'unix:{sock},server=on,wait=off','-no-reboot']
  p=subprocess.Popen(args,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE);s=None
  try:
   deadline=time.monotonic()+60
   while not sock.exists():
    assert p.poll() is None,p.stderr.read()
    if time.monotonic()>deadline:raise TimeoutError('No QMP')
    time.sleep(.05)
   s=socket.socket(socket.AF_UNIX);s.settimeout(5);s.connect(str(sock));f=s.makefile('rwb',buffering=0);f.readline()
   def qmp(cmd,args={}):
    f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
    while True:
     r=json.loads(f.readline())
     if 'error' in r:raise RuntimeError(r)
     if 'return' in r:return
   qmp('qmp_capabilities');capture=w/'vga.bin'
   while True:
    qmp('human-monitor-command',{'command-line':f'pmemsave 0xb8000 4000 "{capture}"'})
    text=capture.read_bytes()[::2].decode(errors='replace')
    if message in text:break
    if time.monotonic()>deadline:raise TimeoutError(text)
    time.sleep(.1)
   qmp('quit');p.wait(timeout=10)
   assert hashlib.sha256(image.read_bytes()).digest()==before
   (w/'result.txt').write_text(kind+': PASS; '+message+'; no filesystem started; image unchanged.\n')
   print((w/'result.txt').read_text().strip(),flush=True)
  finally:
   if s:s.close()
   if p.poll() is None:p.kill();p.wait()
   image.unlink()
if __name__=='__main__':run(sys.argv[1],sys.argv[2])
