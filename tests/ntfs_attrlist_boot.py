#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real kernel/firmware-app extension DATA smoke.
from pathlib import Path
import subprocess,sys,hashlib,struct,zlib
from qemu_smoke import run_qemu
from uefi_smoke import boot
R=Path(__file__).resolve().parents[1];W=R/'build/ntfs2-boot';W.mkdir(exist_ok=True)
fixture=R/'build/ntfs2-tests/attributes.ntfs'
assert fixture.is_file(),'Run make test-ntfs-attrlist first'
image=W/'attributes-unified.img'
if image.exists():image.unlink()
subprocess.run([sys.executable,str(R/'tools/mkunified.py'),'--data-fs','ntfs','--output',str(image)],check=True)
raw=bytearray(image.read_bytes()[:133120*512]);part=fixture.read_bytes();struct.pack_into('<I',raw,462+12,len(part)//512);image.write_bytes(raw+part)
# UEFI configuration disables the panel for serial tests only.
cfg=W/'BOOT.CFG';cfg.write_text((R/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('osk=on','osk=off'))
subprocess.run(['mcopy','-o','-i',str(image)+'@@1048576',str(cfg),'::MULTIOS/BOOT.CFG'],check=True)
commands=['ver','fs','dir','type resident.txt','cmp hardlink.txt resident.txt','crc32 fragmented.bin','crc32 split.bin','crc32 sparse.bin','unlock YES','write resident.txt "forbidden"']
logs=[];report=[]
def verify(text,result,replay=False):
 assert '0.16-dev-unicode' in text and 'NTFS mounted at /' in text and 'Partition base LBA: 133120' in text,text
 assert 'Resident NTFS' in text and 'Identical' in text,text
 # OVMF text-console scroll redraws old lines onto serial; allow only exact RO replays.
 errors=text.count('Error:');readonly=text.count('Error: read-only; backend, driver or mount policy forbids writes')
 assert errors==readonly and (errors>=2 if replay else errors==2),text
 for name in ('fragmented.bin','split.bin','sparse.bin'):
  p=subprocess.run(['ntfscat',str(fixture),'/'+name],capture_output=True,check=True);assert f'{zlib.crc32(p.stdout):08X}' in text,text
 assert result.read_bytes()[133120*512:]==part,'NTFS data changed'
text=run_qemu(image,commands+['exit'],'486',timeout=180);logs.append(text);verify(text,image);report.append('PASS BIOS i386 kernel / emulated 486: resident extension DATA, nonresident list, two DATA extents, sparse read, hardlink and write refusal; NTFS bytes unchanged.')
text,elapsed,result=boot(commands+['poweroff'],W/'uefi',osk=False,source=image,timeout=180);logs.append(text);verify(text,result,True);report.append('PASS UEFI x64 OVMF / emulated USB storage+keyboard firmware: same NTFS extension cases; NTFS bytes unchanged. Prompt %.3fs under emulation, not hardware guarantee.'%elapsed)
(R/'build/phase2-attrlist-boot.log').write_text('\n'.join(logs));(R/'build/phase2-attrlist-boot-report.txt').write_text('\n'.join(report)+'\n');print('\n'.join(report))
