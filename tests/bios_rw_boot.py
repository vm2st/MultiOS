#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — actual native BIOS boot, not a POSIX FS harness.
from pathlib import Path
import subprocess,os,re,time,select,socket,json,shutil,struct,sys,tempfile
r=Path(__file__).resolve().parents[1]
def boot(source,work,commands,cpu='486',memory='32M',timeout=120,stop_at=None,expected_exit=33,crash_command=None,guard_message=None,startup_exit=None):
 work=Path(work);work.mkdir(parents=True,exist_ok=True);image=work/'disk.img';shutil.copyfile(source,image)
 qmp=work/'qmp.sock';qmp.unlink(missing_ok=True)
 qemu=os.environ.get('QEMU_BIOS','qemu-system-i386')
 args=[qemu,'-machine','pc','-cpu',cpu,'-accel','tcg','-m',memory,'-bios',os.environ.get('BIOS_ROM','/usr/share/seabios/bios.bin'),'-L',os.environ.get('QEMU_ROM_DIR','/usr/share/qemu'),'-vga','none','-device','VGA,romfile='+os.environ.get('VGA_ROM','/usr/share/seavgabios/vgabios-stdvga.bin'),'-drive',f'file={image},format=raw,if=ide,index=0','-serial','stdio','-monitor','none','-display','none','-qmp',f'unix:{qmp},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot','-net','none']
 env=os.environ.copy()
 if 'QEMU_BIOS_LIBS' in env:env['LD_LIBRARY_PATH']=env['QEMU_BIOS_LIBS']
 p=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env);data=bytearray();start=time.monotonic()
 def receive(marker,pos=0):
  end=time.monotonic()+timeout
  while marker not in data[pos:]:
   if time.monotonic()>end:raise TimeoutError(data.decode(errors='replace'))
   ready,_,_=select.select([p.stdout],[],[],.1)
   if ready:
    b=os.read(p.stdout.fileno(),8192)
    if not b:raise RuntimeError(data.decode(errors='replace'))
    data.extend(b)
 try:
  if startup_exit is not None:
   tail,_=p.communicate(timeout=timeout);data.extend(tail)
   assert p.returncode==startup_exit,(p.returncode,data[-4000:])
   text=re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',data.decode(errors='replace')).replace('\r','')
   return text,time.monotonic()-start,image
  if guard_message:
   end=time.monotonic()+20
   while not qmp.exists():
    if time.monotonic()>end:raise TimeoutError('QMP not available')
    time.sleep(.05)
   sock=socket.socket(socket.AF_UNIX);sock.connect(str(qmp));f=sock.makefile('rwb',buffering=0);f.readline()
   def qcommand(name,args=None):
    obj={'execute':name}
    if args:obj['arguments']=args
    f.write((json.dumps(obj)+'\n').encode())
    while True:
     a=json.loads(f.readline())
     if 'error' in a:raise RuntimeError(a)
     if 'return' in a:return a['return']
   qcommand('qmp_capabilities');screen=work/'vga.bin'
   while True:
    qcommand('pmemsave',{'val':0xb8000,'size':4000,'filename':str(screen)})
    text=screen.read_bytes()[::2].decode('ascii',errors='replace')
    if guard_message in text:break
    if time.monotonic()>end:raise TimeoutError(text)
    time.sleep(.1)
   qcommand('quit');sock.close();tail,_=p.communicate(timeout=20);data.extend(tail);data.extend(('\nGUARD: '+guard_message+'\n').encode())
   return text,time.monotonic()-start,image
  receive(b'[RO]> ');seconds=time.monotonic()-start
  for cmd in commands:
   pos=len(data);p.stdin.write(cmd.encode()+b'\r');p.stdin.flush()
   if cmd=='exit' or cmd==crash_command:break
   receive(b'> ',pos)
   if stop_at==cmd:break
  if stop_at:
   p.kill();p.wait()
  else:
   tail,_=p.communicate(timeout=timeout);data.extend(tail);assert p.returncode==expected_exit,(p.returncode,data[-4000:])
 finally:
  if p.poll() is None:p.kill();p.wait()
  (work/'console.log').write_text(re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]','',data.decode(errors='replace')).replace('\r',''))
 return (work/'console.log').read_text(),seconds,image

def extract(image,part):
 with image.open('rb') as f:m=f.read(512);a,n=struct.unpack_from('<II',m,470);f.seek(a*512);part.write_bytes(f.read(n*512))
def run(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=180);assert not p.returncode,(a,p.stdout[-1000:],p.stderr[-1000:]);return p.stdout
if __name__=='__main__':
 commands=['ver','fs','unlock YES','write MAIN.TXT "MAIN"','snew MAIN.TXT Notes "abcdef"','swriteat MAIN.TXT notes 2 "XY"','sappend MAIN.TXT NOTES "!"','sresize MAIN.TXT Notes 16','shex MAIN.TXT Notes 0 16','link MAIN.TXT ALIAS.TXT','copyads MAIN.TXT COPIED.TXT','swrite ALIAS.TXT Notes "SHARED"','sread MAIN.TXT Notes 0 6','del MAIN.TXT','sread ALIAS.TXT Notes 0 6','resize COPIED.TXT 8193','writeat COPIED.TXT 4095 "PATCH"','resize COPIED.TXT 5','write AUX.TXT "data"','append AUX.TXT "tail"','ren AUX.TXT RENAMED.TXT','md EMPTY','rd EMPTY','lock','exit']
 for cpu in ('486','pentium','pentium3','qemu64'):
  w=r/'build'/('022-bios-'+cpu);text,seconds,image=boot(r/'build/022-ntfs.img',w,commands,cpu=cpu)
  assert 'Error:' not in text and 'native BIOS i386' in text and 'SHARED' in text and 'Copied and verified DATA + ADS' in text,text[-6000:]
  assert '61 62 58 59 65 66 21 00 00 00 00 00 00 00 00 00' in text,text[-5000:]
  part=w/'ntfs.part';extract(image,part)
  assert run('ntfscat','-n','Notes',part,'/ALIAS.TXT')==b'SHARED'
  assert run('ntfscat','-n','Notes',part,'/COPIED.TXT')==b'abXYef!'+bytes(9)
  assert run('ntfscat',part,'/COPIED.TXT')==b'MAIN\n'
  assert run('ntfscat',part,'/RENAMED.TXT')==b'data\ntail\n'
  run('ntfsfix','-n',part)
  print(f'PASS BIOS/{cpu}: native 32-bit NTFS RW DATA/ADS, hardlink, COPYADS, resize/write-at, directory create/remove, rename; external ntfscat/ntfsfix after stop. Prompt {seconds:.3f}s (emulator only).',flush=True)
 print('PASS 4 BIOS CPU models; i386 ISA compile target, actual 386 hardware NOT tested. Not stable certification.')
