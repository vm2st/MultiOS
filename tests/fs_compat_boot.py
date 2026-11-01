#!/usr/bin/env python3
from pathlib import Path
import os,sys,subprocess,struct,tempfile,hashlib,argparse
a=argparse.ArgumentParser(description='Real BIOS/UEFI filesystem compatibility tests on disposable image copies.')
a.add_argument('images',type=Path);a.add_argument('--active-dir',type=Path);args=a.parse_args()
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tests'))
from uefi_smoke import boot as eboot
from bios_rw_boot import boot as bboot
out=r/'build/fs-compat-boot';out.mkdir(parents=True,exist_ok=True)
qemu_bios=os.environ.get('QEMU_BIOS','qemu-system-i386')
cmd=['ver','modules','fs','unlock YES','write PERSIST.TXT "alpha"','append PERSIST.TXT "beta"','copy PERSIST.TXT TEMP.TXT','cmp PERSIST.TXT TEMP.TXT','resize TEMP.TXT 8193','writeat TEMP.TXT 509 "PATCH"','readat TEMP.TXT 509 5','del TEMP.TXT','sync','lock','poweroff']
for fs in ('fat32','exfat','ntfs'):
 src=args.images/f'MultiOS-0.33-usb-{fs}.img';w=out/('uefi-'+fs)
 text,secs,img=eboot(cmd,w,source=src,osk=False,timeout=180);(w/'console.log').write_text(text)
 assert '0.33-dev-inputfix' in text and 'Error:' not in text and 'Identical' in text and 'PATCH' in text,text[-5000:]
 subprocess.run(['python3',str(r/'tests/usb_result_check.py'),str(img),fs],check=True)
 print(f'PASS delivered UEFI/{fs}: factory config, actual x64 qemu64 OVMF; create/append/copy/cmp/resize/writeat/delete/sync/lock/poweroff; independent persisted bytes. Boot Services retained.',flush=True)
for fs in ('fat32','exfat','ntfs'):
 src=args.images/f'MultiOS-0.33-usb-{fs}.img'
 p=subprocess.run(['python3',str(r/'tests/usb_storage_qemu.py'),str(src),qemu_bios,'ehci','--usb-boot','--reboot','--extended','--padded','--keep-disk'],capture_output=True,timeout=900)
 (out/f'usb-{fs}.log').write_bytes(p.stdout+p.stderr);assert not p.returncode,(p.stdout[-3000:],p.stderr[-3000:])
 disk=r/'build'/f'usb28-storage-MultiOS-0.33-usb-{fs}-ehci-usbboot-extended-padded/disk.img'
 subprocess.run(['python3',str(r/'tests/usb_result_check.py'),str(disk),fs],check=True)
 print(f'PASS native BIOS/EHCI/{fs}: USB-only boot, full extended write suite, reboot persistence, padded tail; independent readback.',flush=True)
for selected in ((0,1) if args.active_dir else ()):
 src=args.active_dir/f'active{selected}.img'
 before=src.read_bytes();start,count=struct.unpack_from('<II',before,470);off=start*512;b=before[off:off+512];reserved=struct.unpack_from('<H',b,14)[0];size=struct.unpack_from('<I',b,36)[0];inactive=(start+reserved+(1-selected)*size)*512
 for platform in ('bios','uefi'):
  w=out/f'active{selected}-{platform}';commands=['ver','readat SEED.BIN 509 8']+cmd[3:]
  if platform=='bios':
   commands[-1]='exit';text,secs,img=bboot(src,w,commands,memory='64M',timeout=180)
  else:text,secs,img=eboot(commands,w,source=src,osk=False,timeout=180)
  (w/'console.log').write_text(text)
  assert 'Error:' not in text and '0.33-dev-inputfix' in text and 'SSSSSSSS' in text and 'Identical' in text,text[-5000:]
  after=img.read_bytes();assert after[inactive:inactive+size*512]==before[inactive:inactive+size*512]
  data=bytearray(after[off:off+count*512]);active=data[(reserved+selected*size)*512:(reserved+(selected+1)*size)*512]
  for k in range(2):data[(reserved+k*size)*512:(reserved+(k+1)*size)*512]=active
  bk=struct.unpack_from('<H',data,50)[0]
  for boot in (0,bk):struct.pack_into('<H',data,boot*512+40,0)
  part=w/'verify.part';part.write_bytes(data)
  p=subprocess.run(['fsck.fat','-n',str(part)],capture_output=True);assert not p.returncode,(p.stdout,p.stderr)
  assert subprocess.check_output(['mtype','-i',str(part),'::PERSIST.TXT'])==b'alpha\nbeta\n'
  print(f'PASS active FAT{selected}/{platform}: native file read and RW, inactive FAT byte-identical; independent selected-FAT projection fsck/readback.',flush=True)
