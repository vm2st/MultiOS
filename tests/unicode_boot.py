#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — real guest BIOS/UEFI binaries, not POSIX host support.
from pathlib import Path
import subprocess,sys,struct,shutil
from qemu_smoke import run_qemu
from uefi_smoke import boot
from sparse_images import digest,copy_range
R=Path(__file__).resolve().parents[1];W=R/'build/unicode-boot';W.mkdir(exist_ok=True);logs=[];report=[]
for kind in ('fat32','exfat','ntfs'):
 image=W/(kind+'.img')
 if image.exists():image.unlink()
 subprocess.run([sys.executable,str(R/'tools/mkunified.py'),'--data-fs','exfat' if kind=='exfat' else 'fat32','--output',str(image)],check=True)
 if kind=='ntfs':
  fixture=R/'build/unicode-tests/ntfs.img';copy_range(fixture,image,133120*512)
  with image.open('r+b') as f:f.seek(462+4);f.write(b'\x07');f.seek(462+12);f.write(struct.pack('<I',fixture.stat().st_size//512))
 cfg=W/'BOOT.CFG';cfg.write_text((R/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('osk=on','osk=off'))
 subprocess.run(['mcopy','-o','-i',str(image)+'@@1048576',str(cfg),'::MULTIOS/BOOT.CFG'],check=True)
 before=digest(image)
 if kind=='ntfs':commands=['ver','dir','type unicode_привет.txt','stat64 Unicode_ПРИВЕТ.txt','unlock YES'];expected_errors=1
 else:commands=['ver','unlock YES','md Документы','write Документы/Привет😀.txt "BIOS-UNICODE"','copy Документы/Привет😀.txt Документы/копия.txt','ren Документы/копия.txt итог.txt','type ДОКУМЕНТЫ/ПРИВЕТ😀.TXT','cmp Документы/Привет😀.txt Документы/итог.txt','lock'];expected_errors=0
 text=run_qemu(image,commands+['exit'],'486',timeout=120);logs.append(text)
 assert '0.16-dev-unicode' in text and text.count('Error:')==expected_errors,text
 assert ('Unicode metadata' if kind=='ntfs' else 'BIOS-UNICODE') in text,text
 if kind=='ntfs':assert digest(image)==before
 report.append('PASS '+kind.upper()+' native BIOS i386/486: UTF-8 serial input, Unicode '+('read-only lookup, original source unchanged' if kind=='ntfs' else 'nested create/copy/rename/casefold/read/compare'))
 # Firmware-provided serial terminals need not decode UTF-8 keyboard input;
 # DIR itself tests the actual UEFI UTF-16 output path with on-disk Unicode names.
 # Direct Unicode SimpleTextInput and surrogate conversion are tested separately
 # by test-uefi-unicode, not inferred from an ASCII USB keyboard.
 commands=['ver','dir'] if kind=='ntfs' else ['ver','tree','fs','unlock YES','write uefi.txt "UEFI-UNICODE-VOLUME"','lock']
 text,elapsed,result=boot(commands+['poweroff'],W/(kind+'-uefi'),osk=False,source=image,timeout=120);logs.append(text)
 assert '0.16-dev-unicode' in text and 'Error:' not in text,text
 # OVMF text serial terminal may transliterate unsupported glyphs; actual decoded
 # CHAR16 cells are covered by the protocol-mock tests, not guessed from a font.
 if kind=='ntfs':assert digest(result)==before
 else:
  text=run_qemu(result,['type ДОКУМЕНТЫ/ПРИВЕТ😀.TXT','type uefi.txt','exit'],'qemu64',timeout=120);logs.append(text)
  assert 'BIOS-UNICODE' in text and 'UEFI-UNICODE-VOLUME' in text and 'Error:' not in text,text
 report.append('PASS '+kind.upper()+' UEFI x64/OVMF firmware: Unicode directory enumeration; '+('NTFS source unchanged' if kind=='ntfs' else 'write on same Unicode-bearing volume, then BIOS re-read')+'; prompt %.3fs emulated, not hardware guarantee.'%elapsed)
 print('\n'.join(report[-2:]),flush=True)
(R/'build/unicode-boot-report.txt').write_text('\n'.join(report)+'\nUEFI Unicode keyboard semantics verified by separate protocol mocks; no firmware-font/hardware/power-loss certification.\n');(R/'build/unicode-boot.log').write_text('\n'.join(logs))
