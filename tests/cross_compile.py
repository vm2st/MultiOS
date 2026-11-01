#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Compile-only: no BSP, linking or boot test. Requires clang in PATH."""
from pathlib import Path
import subprocess,shutil
root=Path(__file__).resolve().parents[1]
cc=shutil.which('clang')
if not cc:raise SystemExit('clang required; this check does not supply ARM/RISC-V board support')
sources=['core/editor.c','core/hardware.c','core/hid.c','core/cpu.c','core/modules.c','core/osk.c','core/hal.c','lib/util.c','lib/fs.c','lib/exfat.c','lib/fat32.c','lib/ntfs.c','lib/ntfs_meta.c','lib/ntfs_attr.c','lib/block64.c','lib/shell.c']
targets={'armv7-none-eabi':['-mcpu=cortex-a7','-mfloat-abi=soft'],'aarch64-none-elf':['-march=armv8-a'],'riscv64-none-elf':['-march=rv64imac','-mabi=lp64']}
lines=[]
for target,extra in targets.items():
 dest=root/'build/cross'/target;dest.mkdir(parents=True,exist_ok=True)
 for f in sources:
  out=dest/(Path(f).stem+'.o')
  subprocess.run([cc,'--target='+target,*extra,'-std=c11','-O1','-ffreestanding','-fno-builtin','-fno-stack-protector','-I'+str(root/'include'),'-I'+str(root/'lib'),'-c',str(root/f),'-o',str(out)],check=True)
 lines.append(f'{target}: {len(sources)} portable-core objects compiled (NOT linked or booted)')
print('\n'.join(lines));(root/'build/cross-report.txt').write_text('\n'.join(lines)+'\n')
