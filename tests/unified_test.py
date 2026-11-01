#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real dual-boot + image editor tests.
from pathlib import Path
import hashlib, os, shutil, subprocess, sys, tempfile, fcntl
from qemu_smoke import run_qemu
from uefi_smoke import boot, ROOT
sys.path.insert(0,str(ROOT/'tools'))
from imagectl import DATA_START, kernel_bytes, efi_bytes
src=ROOT/'build/MultiOS-MultiPulti.img'
editor=[sys.executable,str(ROOT/'tools/imagectl.py')]
checks=0
log=[]
def check(x,msg):
    global checks
    checks+=1
    if not x:raise AssertionError(msg)
def ctl(*args,ok=True):
    p=subprocess.run([*editor,*map(str,args)],capture_output=True,text=True)
    check((p.returncode==0)==ok,p.stdout+p.stderr);return p.stdout
work=ROOT/'build/unified-test';work.mkdir(exist_ok=True)
image=work/'editable.img';shutil.copyfile(src,image)
check(len(kernel_bytes(ROOT/'build/kernel.bin'))<122880,'kernel too big')
check(len(efi_bytes(ROOT/'build/BOOTX64.EFI'))>256,'missing EFI')
ctl('info',image)
cfg=work/'extracted.cfg'
if cfg.exists():cfg.unlink()
ctl('get',image,'MULTIOS/BOOT.CFG',cfg)
check('block=uefi-boot-disk\n' in cfg.read_text(),'wrong unified config')
ctl('get',image,'MULTIOS/BOOT.CFG',cfg,ok=False)
for key,value in [('osk','off'),('block','uefi-boot-disk-ro'),('block','uefi-boot-disk')]:ctl('set',image,key,value)
before=image.read_bytes()
ctl('set',image,'unknown','bad',ok=False);check(image.read_bytes()==before,'failed edit changed image')
with image.open('r+b') as locked:
    fcntl.lockf(locked,fcntl.LOCK_EX|fcntl.LOCK_NB)
    ctl('set',image,'osk','on',ok=False)
check(image.read_bytes()==before,'locked edit changed image')
ctl('put',image,'MULTIOS/KERNEL32.BIN',ROOT/'build/BOOTX64.EFI',ok=False)
check(image.read_bytes()==before,'bad kernel changed image')
ctl('put',image,'EFI/BOOT/BOOTX64.EFI',ROOT/'build/kernel.bin',ok=False)
check(image.read_bytes()==before,'bad EFI changed image')
link=work/'symlink.img'
if link.is_symlink():link.unlink()
link.symlink_to(image)
ctl('info',link,ok=False)
hard=work/'hardlink.img'
if hard.exists():hard.unlink()
os.link(image,hard)
ctl('info',image,ok=False);hard.unlink()
# Actual byte-changing raw kernel replacement, then restore the official bundle.
padded=work/'padded-kernel.bin'; padded.write_bytes(kernel_bytes(ROOT/'build/kernel.bin')+b'\x90')
ctl('put',image,'MULTIOS/KERNEL32.BIN',padded)
with image.open('rb') as f:f.seek(512);check(f.read(padded.stat().st_size)==padded.read_bytes(),'put did not synchronize kernel mirror')
# replace both bundles atomically; shared exFAT bytes must be untouched
before_data=image.read_bytes()[DATA_START*512:]
mbr_tail=image.read_bytes()[440:512]
ctl('install',image,'--boot',ROOT/'build/boot.bin','--kernel',ROOT/'build/kernel.bin','--efi',ROOT/'build/BOOTX64.EFI')
check(image.read_bytes()[440:512]==mbr_tail,'boot update changed disk ID/partition table')
check(image.read_bytes()[DATA_START*512:]==before_data,'binary install changed data partition')
with image.open('rb') as f:f.seek(512);check(f.read(len(kernel_bytes(ROOT/'build/kernel.bin')))==kernel_bytes(ROOT/'build/kernel.bin'),'raw kernel mirror differs')
log.append(run_qemu(image,['ver','credits','modules','unlock YES','md BOTH','write /BOTH/BIOS.TXT "Hello BIOS MultiPulti"','exit'],'486'))
check('vm2 studios' in log[-1] and 'MultiPulti32' in log[-1] and 'Error:' not in log[-1],'native BIOS failed')
decoy=work/'internal-decoy.img';shutil.copyfile(ROOT/'build/multios.img',decoy)
decoy_hash=hashlib.sha256(decoy.read_bytes()).digest()
uefi,seconds,shared=boot(['cpu','ver','credits','modules','type /BOTH/BIOS.TXT','unlock YES','write /BOTH/UEFI.TXT "Hello UEFI MultiPulti"','poweroff'],work/'uefi',osk=False,source=image,decoy=decoy)
check(hashlib.sha256(decoy.read_bytes()).digest()==decoy_hash,'UEFI modified unrelated internal disk')
log.append(uefi)
check('Execution: 64-bit' in uefi and 'block: uefi-boot-disk' in uefi and 'Hello BIOS MultiPulti' in uefi and 'Error:' not in uefi,'shared UEFI failed')
log.append(run_qemu(shared,['ver','type /BOTH/UEFI.TXT','exit'],'qemu64'))
check('Hello UEFI MultiPulti' in log[-1] and 'Error:' not in log[-1],'BIOS cannot read UEFI data')
# force RO via one saved config line, then boot same image
ctl('set',shared,'block','uefi-boot-disk-ro')
ro,_,_=boot(['modules','platform','unlock YES','poweroff'],work/'uefi-ro',osk=False,source=shared)
check('block: uefi-boot-disk-ro' in ro and 'Block write capability: no' in ro and 'Error:' in ro,'disk forced RO failed')
log.append(ro)
part=work/'shared.exfat'
with shared.open('rb') as f,part.open('wb') as out:f.seek(DATA_START*512);shutil.copyfileobj(f,out)
p=subprocess.run(['fsck.exfat','-n',str(part)],capture_output=True,text=True);check(p.returncode==0,p.stdout+p.stderr);log.append(p.stdout+p.stderr)
(ROOT/'build/unified-test.log').write_text(f'{checks} checks passed; BIOS 486 -> UEFI x64 USB -> BIOS qemu64 shared data.\nUEFI prompt {seconds:.3f}s, not hardware guarantee.\n'+'\n'.join(log))
print(f'{checks} unified-image checks passed: atomic editing, bundles, BIOS/UEFI shared data, forced RO, fsck.')
