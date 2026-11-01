#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""UEFI + emulated xHCI USB storage and USB keyboard. No native USB driver claim."""
from pathlib import Path
import subprocess,time,select,os,shutil,json,socket,threading,re
from sparse_images import copy_image
ROOT=Path(__file__).resolve().parents[1]
def boot(commands,workdir,screen=None,timeout=60,config=None,osk=True,failure=None,source=None,decoy=None):
 workdir=Path(workdir);workdir.mkdir(exist_ok=True)
 image=workdir/'usb.img';copy_image(source or ROOT/'build/multios-usb-uefi.img',image)
 if config is not None:
  cfg=workdir/'BOOT.CFG';cfg.write_text(config)
  subprocess.run(['mcopy','-o','-i',str(image)+'@@1048576',str(cfg),'::MULTIOS/BOOT.CFG'],check=True)
 variables=workdir/'vars.fd';shutil.copyfile(os.environ.get('OVMF_VARS','/usr/share/OVMF/OVMF_VARS.fd'),variables)
 sock=workdir/'qmp.sock'
 serialsock=workdir/'serial.sock'
 if serialsock.exists():serialsock.unlink()
 if sock.exists():sock.unlink()
 args=[os.environ.get('QEMU_UEFI','/usr/bin/qemu-system-x86_64'),'-machine','q35','-cpu',os.environ.get('QEMU_UEFI_CPU','qemu64'),'-accel','tcg','-m','256M','-drive',f'if=pflash,format=raw,readonly=on,file={os.environ.get("OVMF_CODE","/usr/share/OVMF/OVMF_CODE.fd")}','-drive',f'if=pflash,format=raw,file={variables}','-device','qemu-xhci,id=xhci','-drive',f'if=none,format=raw,id=stick,file={image}','-device','usb-storage,drive=stick,bus=xhci.0,bootindex=1','-device','usb-kbd,bus=xhci.0','-serial',f'unix:{serialsock},server=on,wait=off','-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-no-reboot']
 if decoy is not None: args.extend(['-drive',f'file={decoy},format=raw,if=ide,index=0'])
 p=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 data=bytearray();pos=0;started=time.monotonic()
 def strip(b):return re.sub(rb'\x1b\[[0-?]*[ -/]*[@-~]',b'',bytes(b))
 serial_input=socket.socket(socket.AF_UNIX)
 deadline=time.monotonic()+10
 while not serialsock.exists():
  if time.monotonic()>deadline:raise RuntimeError('No serial socket')
  time.sleep(.05)
 serial_input.connect(str(serialsock))
 def discard_firmware_serial():
  try:
   while serial_input.recv(65536):pass
  except OSError:pass
 threading.Thread(target=discard_firmware_serial,daemon=True).start()
 try:
  def receive(marker,start=0):
   deadline=time.monotonic()+timeout
   while marker not in strip(data[start:]):
    if time.monotonic()>deadline:raise TimeoutError(data.decode(errors='replace'))
    ready,_,_=select.select([p.stdout],[],[],.1)
    if ready:
     b=os.read(p.stdout.fileno(),8192)
     if not b:raise RuntimeError(data.decode(errors='replace'))
     data.extend(b)
  if failure:
   receive(failure.encode()); elapsed=time.monotonic()-started
   q=socket.socket(socket.AF_UNIX);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
   f.write(b'{"execute":"qmp_capabilities"}\n');f.readline();f.write(b'{"execute":"quit"}\n')
   tail,_=p.communicate(timeout=timeout);data.extend(tail);q.close()
   return strip(data).decode(errors='replace'),elapsed,image
  receive(b'[RO]> ');elapsed=time.monotonic()-started
  # Start-up OSK.ON is present: Ctrl+O hides it before plain serial commands.
  if screen:
   q=socket.socket(socket.AF_UNIX);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline();f.write(b'{"execute":"qmp_capabilities"}\n');f.readline()
   def qmp(name,args):
    f.write((json.dumps({'execute':name,'arguments':args})+'\n').encode())
    while True:
     answer=json.loads(f.readline())
     if 'return' in answer:return answer['return']
     if 'error' in answer:raise RuntimeError(answer)
   qmp('screendump',{'filename':str(screen)})
   # Enter the command VER using ONLY virtual keys selected with arrows/Enter.
   # Initial selection is row=0,col=0. No letter key is sent to the USB keyboard.
   key_sequence=['down']*3+['right']*3+['ret']+['up']*2+['left','ret','right','ret']+['down']*3+['left','ret']
   start=len(data)
   for key in key_sequence:
    qmp('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':35});time.sleep(.07)
   receive(b'> ',start)
   if b'portable C core' not in strip(data[start:]):raise AssertionError('OSK arrow-only VER command did not execute')
   qmp('screendump',{'filename':str(Path(screen).with_name('uefi-osk-typed.ppm'))})
   # Toggle off via the Hide cell, then reopen with Up: no F2/letter keys required.
   for key in ['right','right','ret','up']:
    qmp('send-key',{'keys':[{'type':'qcode','data':key}],'hold-time':35});time.sleep(.12)
   q.close()
  if osk: serial_input.sendall(b'\x0f');time.sleep(.1)
  for cmd in commands:
   pos=len(data);serial_input.sendall(cmd.encode()+b'\r')
   if cmd in ('exit','poweroff','shutdown','reboot'):break
   receive(b'> ',pos)
  tail,_=p.communicate(timeout=timeout);data.extend(tail)
  if p.returncode!=0:raise RuntimeError(f'QEMU exit {p.returncode}: '+data.decode(errors='replace'))
  return strip(data).decode(errors='replace').replace('\r',''),elapsed,image
 finally:
  if p.poll() is None:p.kill();p.wait()
  serial_input.close()
if __name__=='__main__':
 log,seconds,image=boot(['platform','drivers','modules','dir','unlock YES','md USB','write USB/live.txt "Hello UEFI USB"','copy USB/live.txt USB/copy.txt','cmp USB/live.txt USB/copy.txt','type USB/copy.txt','lock','poweroff'],ROOT/'build/uefi-test',ROOT/'build/uefi-osk.ppm')
 (ROOT/'build/uefi-test.log').write_text(f'QEMU/OVMF start to prompt: {seconds:.3f}s (single run, not hardware guarantee)\n'+log)
 assert 'Hello UEFI USB' in log and 'Identical' in log and 'Error:' not in log,log
 print(f'Time to prompt: {seconds:.3f}s; arrow-only OSK VER, USB I/O and POWEROFF passed');print(log[-1800:])
