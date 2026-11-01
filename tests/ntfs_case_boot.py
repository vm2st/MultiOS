#!/usr/bin/env python3
"""Boot current BIOS/UEFI payload with existing POSIX case-collision fixture."""
from pathlib import Path
import sys,subprocess,struct,hashlib,os
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from build_bios_rw_image import install
from bios_rw_boot import boot as bios
from uefi_smoke import boot as uefi
source=Path(sys.argv[1]);original=hashlib.sha256(source.read_bytes()).digest()
fixture=r/'build/ntfs-case-boot-fixture'
subprocess.run([sys.executable,str(r/'tests/ntfs_case.py'),str(source),'--prepare-only'],check=True,env=dict(os.environ,MULTIOS_CASE_WORK=str(fixture)))
w=r/'build/ntfs-case-boot';w.mkdir(exist_ok=True);image=w/'current-ata.img';image.unlink(missing_ok=True)
install(fixture/'seed.img',image,efi=r/'build/BOOTX64.EFI')
commands=['ver','unlock YES','readat File.txt 0 8','readat file.txt 0 8','readat CaseDir/item.bin 0 8','readat casedir/item.bin 0 8','sread Stream.bin Notes 0 5','sread Stream.bin notes 0 11','write file.txt "LOWER"','swrite Stream.bin notes "LOWERADS"','copyatomic File.txt Copied.bin','cmp File.txt Copied.bin','link File.txt Alias.bin','ren File.txt Renamed.txt','del Renamed.txt','readat Alias.bin 0 8','sread Stream.bin Notes 0 5','sread Stream.bin notes 0 8','sync','lock']
for platform in ('bios','uefi'):
 work=w/platform
 if platform=='bios':text,seconds,img=bios(image,work,commands+['exit'],memory='64M',timeout=240)
 else:text,seconds,img=uefi(commands+['poweroff'],work,source=image,osk=False,timeout=240)
 (work/'console.log').write_text(text)
 assert '0.32-dev-exactnames' in text and 'Error:' not in text,text[-7000:]
 for marker in ('AAAAAAAA','bbbbbbbb','UUUUUUUU','dddddddd','FIRST','second-long','LOWERADS','Identical'):assert marker in text,(platform,marker,text[-7000:])
 raw=img.read_bytes();start,count=struct.unpack_from('<II',raw,470);part=work/'verify.part';part.write_bytes(raw[start*512:(start+count)*512])
 p=subprocess.run(['ntfsfix','-n',str(part)],capture_output=True);assert not p.returncode,(p.stdout,p.stderr)
 expected=[('/Alias.bin',b'A'*8193),('/Copied.bin',b'A'*8193),('/file.txt',b'LOWER\n'),('/CaseDir/item.bin',b'U'*4097),('/casedir/item.bin',b'd'*8193)]
 for path,data in expected:assert subprocess.check_output(['ntfscat',str(part),path])==data,(platform,path)
 for name,data in [('Notes',b'FIRST'),('notes',b'LOWERADS')]:assert subprocess.check_output(['ntfscat','-n',name,str(part),'/Stream.bin'])==data
 print(f'PASS exact-name {platform}: actual reads, replace, ADS replace, copy, hardlink, rename, delete; case-distinct DATA/ADS independently verified; ntfsfix -n.',flush=True)
assert hashlib.sha256(source.read_bytes()).digest()==original
print('PASS BIOS i386/486 and UEFI x64 exact-name boots; input image unchanged; no hardware certification.')
