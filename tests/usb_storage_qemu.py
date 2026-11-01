#!/usr/bin/env python3
"""Actual native USB MSC filesystem I/O, serial console only for commands.
Boot IDE is hash-checked for no modifications; mounted filesystem must come from a USB BOT disk.
Usage: usb_storage_qemu.py IMAGE QEMU HOST [--reboot] [--disconnect]
"""
from pathlib import Path
import subprocess,select,time,os,sys,shutil,hashlib,json,socket
ROOT=Path(__file__).resolve().parents[1]
HOSTS={'ehci':['-device','usb-ehci,id=usb'],'ohci':['-device','pci-ohci,id=usb'],'uhci':['-device','piix3-usb-uhci,id=usb']}
def run(image,qemu,host,reboot=False,disconnect=False,usb_boot=False,extended=False,keep=False,padded=False,probe=False,boot_only=False):
 w=ROOT/'build'/('usb28-storage-'+Path(image).stem+'-'+host+('-usbboot' if usb_boot else '')+('-detach' if disconnect else '')+('-extended' if extended else '')+('-padded' if padded else '')+('-probe' if probe else '')+('-bootonly' if boot_only else ''));w.mkdir(parents=True,exist_ok=True)
 boot=w/'boot.img';disk=w/'disk.img';shutil.copyfile(image,boot);shutil.copyfile(image,disk)
 before=hashlib.sha256(boot.read_bytes()).digest();logs=[];original_size=disk.stat().st_size
 if padded:
  with disk.open('r+b') as out:out.truncate(original_size+16*1024*1024);out.seek(-512,2);out.write(b'MultiOS tail sentinel'+bytes(512-len(b'MultiOS tail sentinel')))
  with disk.open('rb') as inp:inp.seek(original_size);tail_before=hashlib.sha256(inp.read()).digest()
 def session(second=False):
  sock=w/'qmp.sock';sock.unlink(missing_ok=True)
  args=[qemu,'-machine','pc','-accel','tcg','-cpu','486','-m','64M',*([] if usb_boot else ['-drive',f'file={boot},format=raw,if=ide,index=0']),'-drive',f'file={disk},format=raw,if=none,id=stick',*HOSTS[host],'-device','usb-storage,drive=stick,bus=usb.0,id=stickdev,removable=on'+(',bootindex=1' if usb_boot else ''),'-boot','order=c','-serial','stdio','-display','none','-monitor','none','-qmp',f'unix:{sock},server=on,wait=off','-device','isa-debug-exit,iobase=0xf4,iosize=0x04','-no-reboot']
  p=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);data=bytearray();q=None
  def receive(marker,start=0,timeout=900):
   end=time.monotonic()+timeout
   while marker not in data[start:]:
    if time.monotonic()>end:raise TimeoutError(bytes(data[-2000:]))
    if select.select([p.stdout],[],[],.1)[0]:
     b=os.read(p.stdout.fileno(),65536)
     if not b:raise RuntimeError(bytes(data[-2000:]))
     data.extend(b)
  def command(cmd,allow_error=False):
   start=len(data);started=time.monotonic();p.stdin.write((cmd+'\n').encode());p.stdin.flush()
   if cmd!='exit':receive(b'> ',start)
   log=bytes(data[start:]).decode(errors='replace').replace('\r','')
   print(f'{Path(image).stem}/{host}: {cmd.split()[0]} {time.monotonic()-started:.2f}s',file=sys.stderr,flush=True)
   if not allow_error:assert 'Error:' not in log,log
   return log
  try:
   receive(b'[RO]> ')
   assert b'mounted at /' in data,data
   inventory=command('modules');assert 'pc-usb-bot' in inventory,inventory
   usb=command('usb');assert 'USB BOT disk ready / RW capable' in usb,usb
   if second:
    assert '\nalpha\nbeta\n' in command('type PERSIST.TXT')
    command('lock')
   elif boot_only:
    command('fs')
   elif probe:
    command('fs');command('dir');command('unlock YES');command('lock')
   else:
    command('unlock YES');command('mkdir USB27');command('write USB27/A.TXT "alpha"');command('append USB27/A.TXT "beta"')
    if extended:
     command('touch LARGE.BIN');command('resize LARGE.BIN 8193')
     command('writeat LARGE.BIN 509 "BOUNDARY"');command('writeat LARGE.BIN 4093 "CLUSTER"')
     assert '\nBOUNDARY\n' in command('readat LARGE.BIN 509 8')
     assert '\nCLUSTER\n' in command('readat LARGE.BIN 4093 7')
     command('copy LARGE.BIN LARGE2.BIN');assert 'identical' in command('cmp LARGE.BIN LARGE2.BIN').lower()
     command('resize LARGE.BIN 513');command('resize LARGE.BIN 8193')
     assert '00 00 00 00 00 00 00 00' in command('hexat LARGE.BIN 513 8')
     command('del LARGE.BIN');command('del LARGE2.BIN')
     if b'NTFS mounted' in data:
      command('write ADS.TXT "MAIN"');command('snew ADS.TXT Notes "abcdef"');command('swriteat ADS.TXT notes 2 "XY"')
      command('sappend ADS.TXT NOTES "!"');command('sresize ADS.TXT Notes 16')
      assert '61 62 58 59 65 66 21 00 00 00 00 00 00 00 00 00' in command('shex ADS.TXT Notes 0 16')
      command('link ADS.TXT ALIAS.TXT');command('copyads ADS.TXT COPIED.TXT')
      command('swrite ALIAS.TXT Notes "SHARED"');command('del ADS.TXT')
      assert '\nSHARED\n' in command('sread ALIAS.TXT Notes 0 6')

    assert '\nalpha\nbeta\n' in command('type USB27/A.TXT')
    command('copy USB27/A.TXT USB27/B.TXT');assert 'identical' in command('cmp USB27/A.TXT USB27/B.TXT').lower()
    command('ren USB27/B.TXT C.TXT');command('resize USB27/C.TXT 6');command('writeat USB27/C.TXT 0 "ALPHA"')
    assert '\nALPHA\n' in command('type USB27/C.TXT')
    command('copy USB27/A.TXT PERSIST.TXT');command('del USB27/A.TXT');command('del USB27/C.TXT');command('rmdir USB27');command('sync');command('lock')
    if disconnect:
     q=socket.socket(socket.AF_UNIX);q.settimeout(10);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
     def qmp(cmd,args={}):
      f.write((json.dumps({'execute':cmd,'arguments':args})+'\n').encode())
      while True:
       r=json.loads(f.readline())
       if 'return' in r:return
       if 'error' in r:raise RuntimeError(r)
     qmp('qmp_capabilities');qmp('device_del',{'id':'stickdev'});time.sleep(1)
     assert 'disconnected' in command('usb').lower()
     assert 'Error:' in command('type PERSIST.TXT',True)
     assert 'Error:' in command('unlock YES',True)
     assert 'Error:' in command('write MUSTFAIL.TXT \"no\"',True)
   if disconnect:
    # EXIT requires a successful flush; an offline disk must not fake success.
    qmp('quit');tail,_=p.communicate(timeout=30);data.extend(tail);assert p.returncode==0,p.returncode
   else:
    command('exit',True);tail,_=p.communicate(timeout=30);data.extend(tail);assert p.returncode==33,p.returncode
  finally:
   if p.poll() is None:p.kill();p.wait()
   if q:q.close()
   logs.append(bytes(data));(w/('reboot.log' if second else 'console.log')).write_bytes(data)
 session()
 if reboot and not disconnect:session(True)
 assert hashlib.sha256(boot.read_bytes()).digest()==before,'IDE boot source changed'
 if boot_only:print(f'{Path(image).stem}/{host}: PASS native USB boot, selected BOT, mounted RO, bounded frame startup, clean EXIT.',flush=True)
 elif probe:print(f'{Path(image).stem}/{host}: PASS aligned larger-media prefix, native mount/DIR/guarded UNLOCK/LOCK, no IDE.',flush=True)
 else:print(f'{Path(image).stem}/{host}: PASS native USB BOT mount, mkdir/write/append/copy/cmp/rename/resize/writeat/delete/sync'+(', reboot persistence' if reboot and not disconnect else '')+(', detach fail-closed' if disconnect else '')+(', BIOS USB boot with no IDE disk' if usb_boot else '')+(', cross-sector/cluster/zero-regrow'+('/NTFS ADS-hardlink-COPYADS' if 'ntfs' in Path(image).stem else '') if extended else '')+('; no IDE device attached.' if usb_boot else '; IDE boot unchanged.'),flush=True)
 if padded:
  with disk.open('rb') as inp:inp.seek(original_size);assert hashlib.sha256(inp.read()).digest()==tail_before,'USB tail changed'
 boot.unlink()
 if not keep:disk.unlink()
if __name__=='__main__':run(sys.argv[1],sys.argv[2],sys.argv[3], '--reboot' in sys.argv,'--disconnect' in sys.argv,'--usb-boot' in sys.argv,'--extended' in sys.argv,'--keep-disk' in sys.argv,'--padded' in sys.argv,'--probe-only' in sys.argv,'--boot-only' in sys.argv)
