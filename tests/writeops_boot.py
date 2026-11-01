#!/usr/bin/env python3
"""Real BIOS/ATA + UEFI boots of seeded writeops fixtures with current payload.
Usage: writeops_boot.py DIRECTORY_WITH_fat32.img_exfat.img_ntfs.img
Prepare fixtures using fs_writeops.py and install the current BIOS+EFI bundle
with build_bios_rw_image.py (its default ATA profile is used for BIOS).
"""
from pathlib import Path
import sys,subprocess,struct
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tests'))
from bios_rw_boot import boot as bios
from uefi_smoke import boot as uefi
source=Path(sys.argv[1]);work=root/'build/writeops-boot';work.mkdir(exist_ok=True)
commands=['ver','unlock YES','copyatomic ReadOnly.txt CopyRO.bin','cmp ReadOnly.txt CopyRO.bin','writeat CopyRO.bin 509 "LOCAL"','readat CopyRO.bin 509 5','ren MixedCase.txt mixedcase.TXT','ren MixedDir mixeddir','dir','type mixeddir/child.txt','sync','lock']
for fs in ('fat32','exfat','ntfs'):
 for platform in ('bios','uefi'):
  directory=work/(fs+'-'+platform);sequence=commands+(['exit'] if platform=='bios' else ['poweroff'])
  if platform=='bios':text,seconds,image=bios(source/(fs+'.img'),directory,sequence,memory='64M',timeout=180)
  else:text,seconds,image=uefi(sequence,directory,source=source/(fs+'.img'),osk=False,timeout=180)
  (directory/'console.log').write_text(text)
  assert '0.32-dev-exactnames' in text and 'Error:' not in text and 'Identical' in text and 'LOCAL' in text and 'nested' in text and 'mixedcase.TXT' in text and 'mixeddir' in text,text[-5000:]
  data=image.read_bytes();start,count=struct.unpack_from('<II',data,470);part=directory/'verify.part';part.write_bytes(data[start*512:(start+count)*512])
  checker='fsck.fat' if fs=='fat32' else 'fsck.exfat' if fs=='exfat' else 'ntfsfix'
  p=subprocess.run([checker,'-n',str(part)],capture_output=True);assert not p.returncode,(p.stdout,p.stderr)
  if fs in ('fat32','ntfs'):
   for path in ('ReadOnly.txt','CopyRO.bin'):
    args=['mtype','-i',str(part),'::'+path] if fs=='fat32' else ['ntfscat',str(part),'/'+path]
    p=subprocess.run(args,capture_output=True);expected=bytearray((i*37+11)&255 for i in range(8193))
    if path=='CopyRO.bin':expected[509:514]=b'LOCAL'
    assert not p.returncode and p.stdout==expected,(fs,path,p.stderr)
  # Same production VFS checks exact full file content on exFAT as on FAT/NTFS;
  # independent exFAT tool above validates structure only, not its file bytes.
  for path in ('/ReadOnly.txt','/CopyRO.bin'):
   p=subprocess.run([str(root/'build/fs-read-probe'),str(image),path,'0','8193'],capture_output=True)
   expected=bytearray((i*37+11)&255 for i in range(8193))
   if path=='/CopyRO.bin':expected[509:514]=b'LOCAL'
   assert not p.returncode and p.stdout==expected,(fs,path,p.stderr)
  print(f'PASS {fs}/{platform}: actual atomic protected-source copy, independent writable destination, case-only file/directory rename, preserved nested data, exact source/copy VFS readback and external read-only checker'+(' + external exact bytes' if fs!='exfat' else ''),flush=True)
print('PASS 3 filesystems x BIOS/ATA and UEFI firmware; not hardware certification or native UEFI xHCI support.')
