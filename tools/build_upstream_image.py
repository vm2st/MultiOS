#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — require actual upstream engines, no silent fallback.
from pathlib import Path
import argparse,subprocess,tempfile
from mkunified import build,ROOT
from prepare_journal import prepare
p=argparse.ArgumentParser(description='NEW unpublished image: BIOS FatFs + NTFS RO, UEFI FatFs + experimental NTFS-3G RW')
p.add_argument('--output',type=Path,required=True);p.add_argument('--data-fs',choices=('exfat','fat32','ntfs'),default='exfat');a=p.parse_args()
if a.output.exists() or a.output.is_symlink():p.error('Output already exists; choose a NEW filename')
for name,symbols in [('kernel.elf',('f_open','multi_fatfs_put','multi_storage_prepare')),('multios-uefi.so',('ntfs_attr_pwrite','multi_ntfs3g_put','multi_fatfs_put','multi_storage_prepare'))]:
 s=subprocess.check_output(['nm',str(ROOT/'build'/name)],text=True)
 for symbol in symbols:
  if not any(line.split()[-1:]==[symbol] for line in s.splitlines()):p.error(f'{name}: missing {symbol}; use make upstream-image')
if subprocess.check_output(['nm','-u',str(ROOT/'build/multios-uefi.so')],text=True).strip():p.error('Unresolved firmware symbol: refusing unbootable image')
sections=subprocess.check_output(['objdump','-h',str(ROOT/'build/BOOTX64.EFI')],text=True)
if '.reloc' not in sections:p.error('EFI relocation section missing; refusing unbootable image')
if 'pei-x86-64' not in sections:p.error('Expected x64 PE/COFF EFI executable')
with tempfile.TemporaryDirectory(prefix='upstream-image-',dir=ROOT/'build') as td:
 source=Path(td)/'source.img';build(source,a.data_fs);prepare(source,a.output,2,experimental_ntfs=a.data_fs=='ntfs',journal_records=4096)
print('Created NEW unpublished upstream image. Data='+a.data_fs+'; BIOS NTFS RO; UEFI NTFS RW EXPERIMENTAL; NOT Release 1.0.')
