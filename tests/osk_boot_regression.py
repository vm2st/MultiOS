#!/usr/bin/env python3
"""Disposable BIOS/QEMU console regression; reads actual guest VGA text memory."""
from pathlib import Path
import argparse,os,subprocess,shutil,time,select,socket,json,re
p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
w=a.output.resolve();w.mkdir(parents=True,exist_ok=True);disk=w/'disk.img';shutil.copyfile(a.image,disk);qmp=w/'qmp.sock';qmp.unlink(missing_ok=True)
args=[os.environ.get('QEMU_BIOS','qemu-system-i386'),'-machine','pc','-cpu','486','-accel','tcg','-m','64M','-bios',os.environ.get('BIOS_ROM','/usr/share/seabios/bios.bin'),'-L',os.environ.get('QEMU_ROM_DIR','/usr/share/qemu'),'-vga','none','-device','VGA,romfile='+os.environ.get('VGA_ROM','/usr/share/seavgabios/vgabios-stdvga.bin'),'-drive',f'file={disk},format=raw,if=ide,index=0','-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{qmp},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot','-net','none']
guest=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);data=bytearray()
def receive(marker,pos=0):
 end=time.monotonic()+90
 while marker not in data[pos:]:
  if time.monotonic()>end:raise TimeoutError(data[-2000:])
  if select.select([guest.stdout],[],[],.1)[0]:
   b=os.read(guest.stdout.fileno(),8192)
   if not b:raise RuntimeError(data[-2000:])
   data.extend(b)
def send(s):guest.stdin.write(s);guest.stdin.flush()
try:
 receive(b'[RO]> ')
 q=socket.socket(socket.AF_UNIX);q.connect(str(qmp));f=q.makefile('rwb',buffering=0);f.readline()
 def command(name,args=None):
  obj={'execute':name}
  if args is not None:obj['arguments']=args
  f.write((json.dumps(obj)+'\n').encode())
  while True:
   obj=json.loads(f.readline())
   if 'error' in obj:raise RuntimeError(obj)
   if 'return' in obj:return obj['return']
 command('qmp_capabilities')
 for i in range(25):
  pos=len(data);send(b'echo ROW\r');receive(b'[RO]> ',pos)
 pos=len(data);send(b'echo PRESERVE');receive(b'PRESERVE',pos);time.sleep(.2)
 command('send-key',{'keys':[{'type':'qcode','data':'f2'}],'hold-time':40});time.sleep(.3)
 dump=w/'vga.bin';command('pmemsave',{'val':0xb8000,'size':4000,'filename':str(dump)})
 text=dump.read_bytes()[::2].decode('ascii',errors='replace');line=text[17*80:18*80]
 assert 'MultiOS:/ [RO]> echo PRESERVE' in line,line
 assert 'OSK | F6:' in text[18*80:19*80],text
 command('send-key',{'keys':[{'type':'qcode','data':'f2'}],'hold-time':40});time.sleep(.2)
 pos=len(data);send(b'\x02X\r');receive(b'[RO]> ',pos);assert b'\nPRESERVXE\r\n' in data[pos:],data[pos:]
 send(b'exit\r');tail,_=guest.communicate(timeout=90);data.extend(tail);assert guest.returncode==33,guest.returncode
 q.close();print('PASS actual BIOS/486: PS2 F2 opens/closes OSK at bottom; VGA row 17 retains command; mid-line insertion after toggle executes correct command')
finally:
 if guest.poll() is None:guest.kill();guest.wait()
 (w/'console.log').write_text(re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',data.decode(errors='replace')).replace('\r',''))
