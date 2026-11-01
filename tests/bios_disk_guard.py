#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — deliberately boot IDE slave, not primary master.
from pathlib import Path
import hashlib, json, os, shutil, socket, subprocess, time
from uefi_smoke import ROOT
work=ROOT/'build/bios-disk-guard';work.mkdir(exist_ok=True)
boot=work/'boot.img';decoy=work/'decoy.img'
shutil.copyfile(ROOT/'build/MultiOS-MultiPulti.img',boot)
shutil.copyfile(ROOT/'build/multios.img',decoy)
before=hashlib.sha256(decoy.read_bytes()).digest()
sock=work/'qmp.sock'
if sock.exists():sock.unlink()
qemu=os.environ.get('QEMU','qemu-system-x86_64')
args=[qemu,'-machine','pc','-accel','tcg','-m','32M',
    '-drive',f'file={decoy},format=raw,if=ide,index=0',
    '-drive',f'file={boot},format=raw,if=none,id=bootdrive',
    '-device','ide-hd,drive=bootdrive,bus=ide.0,unit=1,bootindex=1',
    '-boot','strict=on','-display','none','-serial','none','-monitor','none',
    '-qmp',f'unix:{sock},server=on,wait=off']
p=subprocess.Popen(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
try:
    deadline=time.monotonic()+10
    while not sock.exists():
        if p.poll() is not None:raise RuntimeError(p.communicate()[0].decode(errors='replace'))
        if time.monotonic()>deadline:raise TimeoutError('QMP missing')
        time.sleep(.05)
    q=socket.socket(socket.AF_UNIX);q.connect(str(sock));f=q.makefile('rwb',buffering=0);f.readline()
    def command(name,arguments=None):
        f.write((json.dumps({'execute':name,'arguments':arguments or {}})+'\n').encode())
        while True:
            a=json.loads(f.readline())
            if 'return' in a:return a['return']
            if 'error' in a:raise RuntimeError(a)
    command('qmp_capabilities')
    mem=work/'vga.bin';deadline=time.monotonic()+12;text=''
    while time.monotonic()<deadline:
        time.sleep(.2)
        command('human-monitor-command',{'command-line':f'pmemsave 0xb8000 320 "{mem}"'})
        if mem.exists():text=mem.read_bytes()[::2].decode('ascii',errors='replace')
        if 'pc-ata-pio' in text:break
    assert 'MultiPulti / vm2 studios: module boot failed' in text and 'pc-ata-pio' in text,text
    command('quit');p.communicate(timeout=10);q.close()
    assert hashlib.sha256(decoy.read_bytes()).digest()==before
    (ROOT/'build/bios-disk-guard.log').write_text('BIOS boot disk identity guard passed.\n'+text+'\nUnrelated primary-master image unchanged.\n')
    print('BIOS identity guard passed: booting IDE slave cannot mount unrelated primary master.')
finally:
    if p.poll() is None:p.kill();p.wait()
