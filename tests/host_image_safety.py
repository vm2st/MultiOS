#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — regular-file/locking tests on disposable paths.
from pathlib import Path
import os,fcntl,tempfile,subprocess,shutil,hashlib,sys
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import imagectl
checks=0;lines=[]
with tempfile.TemporaryDirectory() as td:
 td=Path(td);image=td/'work.img';shutil.copyfile(ROOT/'build/multios.img',image)
 host=[str(ROOT/'build/multios-host')]
 def run(path,write=False):return subprocess.run([*host,str(path),*(['--allow-write'] if write else [])],input='exit\n',text=True,capture_output=True,timeout=3)
 fifo=td/'pipe';os.mkfifo(fifo)
 assert run(fifo).returncode==2;checks+=1
 for function in (imagectl.kernel_bytes,imagectl.efi_bytes,imagectl.boot_bytes):
  try:function(fifo)
  except ValueError:checks+=1
  else:raise AssertionError('FIFO accepted')
 link=td/'hard.img';os.link(image,link);assert run(image,True).returncode==2;checks+=1;link.unlink()
 symlink=td/'sym.img';symlink.symlink_to(image);assert run(symlink).returncode==2;checks+=1
 with image.open('r+b') as locked:
  fcntl.lockf(locked,fcntl.LOCK_EX|fcntl.LOCK_NB)
  assert run(image,True).returncode==2 and run(image).returncode==2;checks+=2
 with image.open('rb') as locked:
  fcntl.lockf(locked,fcntl.LOCK_SH|fcntl.LOCK_NB)
  assert run(image,True).returncode==2 and run(image).returncode==0;checks+=2
 huge=td/'huge.bin'
 with huge.open('wb') as f:f.truncate(2*1024*1024*1024)
 for function in (imagectl.kernel_bytes,imagectl.efi_bytes,imagectl.boot_bytes):
  try:function(huge)
  except ValueError:checks+=1
  else:raise AssertionError('Oversized source accepted')
 # Unrelated path does not replace a working copy on invalid update.
 unified=td/'unified.img';shutil.copyfile(ROOT/'build/MultiOS-MultiPulti.img',unified)
 before=hashlib.sha256(unified.read_bytes()).digest()
 p=subprocess.run([sys.executable,str(ROOT/'tools/imagectl.py'),'put',str(unified),'MULTIOS/KERNEL32.BIN',str(fifo)],capture_output=True,text=True,timeout=5)
 assert p.returncode!=0 and hashlib.sha256(unified.read_bytes()).digest()==before;checks+=1
 # A source changed immediately after reading must not split raw and FAT copies.
 source=td/'kernel.bin';expected=(ROOT/'build/kernel.bin').read_bytes();source.write_bytes(expected)
 original=imagectl.inject_data
 def change_source(img,rel,data):
  source.write_bytes(b'X'*32);original(img,rel,data)
 imagectl.inject_data=change_source
 try:imagectl.update_kernel(unified,source)
 finally:imagectl.inject_data=original
 out=td/'mirror.bin';imagectl.extract(unified,'MULTIOS/KERNEL32.BIN',out)
 with unified.open('rb') as f:f.seek(512);assert f.read(len(expected))==expected;checks+=1
 assert out.read_bytes()==expected and source.read_bytes()!=expected;checks+=1
 # Bad FAT is rejected before any part of the original transaction is replaced.
 with unified.open('r+b') as f:f.seek(2048*512+512*32);f.write(bytes(12))
 before=hashlib.sha256(unified.read_bytes()).digest()
 try:imagectl.transaction(unified,lambda img:imagectl.inject_data(img,'MULTIOS/BRAND.TXT',b'test'))
 except (subprocess.CalledProcessError,ValueError):pass
 else:raise AssertionError('Corrupt FAT update accepted')
 assert hashlib.sha256(unified.read_bytes()).digest()==before;checks+=1
report=f'{checks} host/image safety assertions PASS: FIFO/symlink/hardlink rejection, record-lock interoperability, bounded sources, stable kernel snapshot/raw mirror, corrupt-FAT transactional refusal. Advisory locks do not stop malicious lock-ignoring writers.\n'
(ROOT/'build/dev10-host-safety-report.txt').write_text(report);print(report)
