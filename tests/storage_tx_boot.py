#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — experimental transactional BIOS + UEFI smoke.
from pathlib import Path
import sys,shutil,subprocess,os,re
from qemu_smoke import run_qemu
from uefi_smoke import boot
R=Path(__file__).resolve().parents[1];W=R/'build/storage-tx-boot';W.mkdir(exist_ok=True);logs=[];report=[]
source=Path(os.environ.get('MULTIOS_TX_IMAGE',str(R/'build/MultiOS-TX-EXPERIMENTAL.img')))
qemu=os.environ.get('QEMU') or shutil.which('qemu-system-x86_64')
machines=subprocess.run([qemu,'-machine','help'],capture_output=True,text=True,check=True).stdout
bios_supported=bool(re.search(r'^pc\s',machines,re.M))
if not bios_supported:report.append('SKIP BIOS runtime: installed QEMU lacks pc/i440fx + legacy IDE. BIOS kernel compiled; no native BIOS RW boot validation in this environment.')
else:
 for cpu in ('486','qemu64'):
  image=W/(cpu+'.img');shutil.copyfile(source,image)
  text=run_qemu(image,['ver','fs','unlock YES','md TEST','write TEST/a.txt "TX-BIOS"','append TEST/a.txt "APPEND"','copy TEST/a.txt TEST/b.txt','cmp TEST/a.txt TEST/b.txt','ren TEST/b.txt result.txt','type TEST/result.txt','del TEST/result.txt','del TEST/a.txt','rd TEST','lock','exit'],cpu,timeout=120)
  logs.append(text);assert 'Error:' not in text and 'Mount failed' not in text and 'Private undo' in text and 'Identical' in text and 'TX-BIOS\nAPPEND' in text,text
  report.append('PASS BIOS '+cpu+': prepared unified image, create/append/copy/rename/read/delete/rmdir via transaction profile')
config=(R/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('osk=on','osk=off')
text,elapsed,result=boot(['ver','fs','unlock YES','write uefi-tx.txt "TX-UEFI"','copy uefi-tx.txt uefi-copy.txt','cmp uefi-tx.txt uefi-copy.txt','lock','poweroff'],W/'uefi',osk=False,config=config,source=source,timeout=120)
logs.append(text);assert 'Error:' not in text and 'Private undo' in text and 'Identical' in text,text
if bios_supported:
 text=run_qemu(result,['type uefi-tx.txt','type uefi-copy.txt','fs','exit'],'qemu64',timeout=120);logs.append(text);assert 'Error:' not in text and text.count('TX-UEFI')>=2,text
else:
 p=subprocess.run([str(R/'build/multios-host'),str(result)],input='type uefi-tx.txt\ntype uefi-copy.txt\nexit\n',text=True,capture_output=True,check=True)
 logs.append(p.stdout+p.stderr);assert 'Error:' not in p.stdout and p.stdout.count('TX-UEFI')>=2,p.stdout

report.append('PASS UEFI x64/OVMF: xHCI USB storage + firmware Block I/O, transaction create/copy/compare; re-read after poweroff (BIOS if available, otherwise POSIX harness). Firmware services retained, not native xHCI storage driver.')
report.append('UEFI prompt %.3fs under emulation; not hardware/boot-time guarantee.'%elapsed)
(R/'build/storage-tx-boot.log').write_text('\n'.join(logs));(R/'build/storage-tx-boot-report.txt').write_text('\n'.join(report)+'\n');print('\n'.join(report))
