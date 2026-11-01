#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — BIOS/UEFI range reads beyond 4GiB.
from pathlib import Path
import struct,sys,subprocess
from qemu_smoke import run_qemu
from uefi_smoke import boot
from sparse_images import copy_range,digest
R=Path(__file__).resolve().parents[1];W=R/'build/fs64-boot';W.mkdir(exist_ok=True);logs=[];report=[]
sys.path.insert(0,str(R/'tools'));from mkimage import checksum
for kind in ('fat32','exfat','ntfs'):
 image=W/(kind+'.img')
 if image.exists():image.unlink()
 subprocess.run([sys.executable,str(R/'tools/mkunified.py'),'--data-fs','fat32','--output',str(image)],check=True)
 if kind!='fat32':
  part=R/'build/fs64-tests'/(kind+'.img');copy_range(part,image,133120*512)
  with image.open('r+b') as f:
   f.seek(462+4);f.write(b'\x07');f.seek(462+12);f.write(struct.pack('<I',part.stat().st_size//512))
   if kind=='exfat':
    for region in (0,12):
     off=(133120+region)*512;f.seek(off);b=bytearray(f.read(11*512));struct.pack_into('<Q',b,64,133120);f.seek(off);f.write(b);f.write(struct.pack('<I',checksum(b,(106,107,112)))*128)
 cfg=(R/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('osk=on','osk=off')
 tmp=W/'BOOT.CFG';tmp.write_text(cfg);subprocess.run(['mcopy','-o','-i',str(image)+'@@1048576',str(tmp),'::MULTIOS/BOOT.CFG'],check=True)
 before=digest(image)
 if kind=='fat32':commands=['ver','stat64 WELCOME.TXT','readat WELCOME.TXT 0 7','hexat WELCOME.TXT 0 7'];wanted=['Extended read-only view','MultiOS']
 else:commands=['ver','stat64 wide.bin','readat wide.bin 0xfffffff8 16','hexat wide.bin 0x100000000 8','readat wide.bin 5368709227 16'];wanted=['Size64: 5368709243','CROSS-4G-BOUNDAR','0000000100000000','END-OF-WIDE-FILE']
 text=run_qemu(image,commands+['exit'],'486',timeout=180);logs.append(text)
 assert '0.16-dev-unicode' in text and 'Error:' not in text and all(s in text for s in wanted),text
 assert digest(image)==before,'BIOS changed source'
 text,elapsed,result=boot(commands+['poweroff'],W/(kind+'-uefi'),osk=False,source=image,timeout=180);logs.append(text)
 assert '0.16-dev-unicode' in text and 'Error:' not in text and all(s in text for s in wanted),text
 assert digest(result)==before,'UEFI changed source'
 report.append('PASS '+kind.upper()+' BIOS i386/486 + UEFI x64 OVMF: '+('regular-file wide API' if kind=='fat32' else '5GiB file, >4GiB range and EOF marker')+'; original and copied sparse image extents byte-identical. UEFI prompt %.3fs emulated, not hardware guarantee.'%elapsed);print(report[-1],flush=True)
(R/'build/fs64-boot.log').write_text('\n'.join(logs));(R/'build/fs64-boot-report.txt').write_text('\n'.join(report)+'\n')
