#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — one-image experimental transactional build.
"""Create NEW BIOS + x64 UEFI image with a protected FAT32/exFAT data partition.
Requires compiled transactional binaries. Never modifies an existing output.
"""
from pathlib import Path
import argparse,subprocess,tempfile
from mkunified import build,ROOT
from prepare_journal import prepare

def create(output,kind):
 output=Path(output)
 if output.exists() or output.is_symlink():raise ValueError('Choose a NEW output filename')
 for name in ('kernel.elf','multios-uefi.so'):
  p=subprocess.run(['nm',str(ROOT/'build'/name)],capture_output=True,text=True,check=True)
  if ' multi_storage_prepare' not in p.stdout:raise ValueError('Transactional BIOS and UEFI binaries required: use make journal-image')
 with tempfile.TemporaryDirectory(prefix='tx-image-',dir=ROOT/'build') as td:
  source=Path(td)/'unprepared.img';build(source,kind);prepare(source,output,2)
 print(str(output)+': BIOS i386 + UEFI x64; private undo protects data partition 2; NTFS remains RO; EXPERIMENTAL, UNPUBLISHED, NOT RELEASE 1.0')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);p.add_argument('--data-fs',choices=('fat32','exfat'),default='exfat');a=p.parse_args()
 try:create(a.output,a.data_fs)
 except (ValueError,OSError,subprocess.CalledProcessError) as e:p.exit(1,str(e)+'\n')
