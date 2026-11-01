#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — same FAT32/NTFS backends in native BIOS and UEFI.
from pathlib import Path
import sys,subprocess,shutil,hashlib
from qemu_smoke import run_qemu
from uefi_smoke import boot
ROOT=Path(__file__).resolve().parents[1];work=ROOT/'build/fs11-boot';work.mkdir(exist_ok=True)
lines=[];logs=[]
for kind in ('fat32','ntfs'):
 source=work/(kind+'.img')
 if source.exists():source.unlink()
 subprocess.run([sys.executable,str(ROOT/'tools/mkunified.py'),'--data-fs',kind,'--output',str(source)],check=True)
 # Disable initial panel for serial-only smoke; keyboard accessibility tested separately.
 cfg=work/'BOOT.CFG';cfg.write_text((ROOT/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('osk=on','osk=off'))
 subprocess.run(['mcopy','-o','-i',str(source)+'@@1048576',str(cfg),'::MULTIOS/BOOT.CFG'],check=True)
 baseline=source.read_bytes()[133120*512:]
 bios=work/(kind+'-bios.img');shutil.copyfile(source,bios)
 commands=['fs','type WELCOME.TXT']
 if kind=='fat32':commands+=['unlock YES','md TEST','write TEST/a.txt "BIOS FAT32 write"','copy TEST/a.txt TEST/b.txt','cmp TEST/a.txt TEST/b.txt','lock']
 else:commands+=['unlock YES','touch denied']
 text=run_qemu(bios,commands+['exit'],'486',timeout=90);logs.append(text)
 assert kind.upper()+' mounted at /' in text and 'Partition base LBA: 133120' in text,text
 assert text.count('Error:')==(0 if kind=='fat32' else 2),text
 if kind=='ntfs':assert bios.read_bytes()[133120*512:]==baseline
 lines.append(kind.upper()+' BIOS i386/486: PASS, correct data partition, read'+('/write+copy' if kind=='fat32' else '/write refusal; data byte-identical'))
 print(lines[-1],flush=True)
 commands=['fs','type WELCOME.TXT']
 if kind=='fat32':commands+=['type TEST/a.txt','unlock YES','write TEST/uefi.txt "UEFI FAT32 write"','lock']
 else:commands+=['unlock YES','write denied.txt "no"']
 text,elapsed,result=boot(commands+['poweroff'],work/(kind+'-uefi'),osk=False,source=bios,timeout=90);logs.append(text)
 assert kind.upper()+' mounted at /' in text and 'Partition base LBA: 133120' in text,text
 assert text.count('Error:')==(0 if kind=='fat32' else 2),text
 if kind=='fat32':
  assert 'BIOS FAT32 write' in text
  text=run_qemu(result,['type TEST/uefi.txt','exit'],'qemu64',timeout=90);logs.append(text);assert 'UEFI FAT32 write' in text and 'Error:' not in text,text
  part=work/'fat32-data.part';part.write_bytes(result.read_bytes()[133120*512:]);p=subprocess.run(['fsck.fat','-n',str(part)],text=True,capture_output=True);logs.append(p.stdout+p.stderr);assert p.returncode==0,p.stdout+p.stderr
 else:assert result.read_bytes()[133120*512:]==baseline
 lines.append(kind.upper()+f' UEFI x64/OVMF USB firmware: PASS; prompt {elapsed:.3f}s (emulated, not a hardware promise).'+(' Shared FAT32 BIOS -> UEFI -> BIOS contents and fsck clean.' if kind=='fat32' else ' NTFS write attempts refused; data partition byte-identical.'))
 print(lines[-1],flush=True)
(ROOT/'build/dev11-filesystems-boot-report.txt').write_text('\n'.join(lines)+'\nNo physical hardware certification. UEFI retains Boot Services.\n')
(ROOT/'build/dev11-filesystems-boot-logs.txt').write_text('\n'.join(logs))
