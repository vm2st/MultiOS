#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — interactive QEMU launcher, image COPY only.
import argparse,os,shutil,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description='UEFI x64 QEMU launch on a temporary copy; changes are discarded on exit')
p.add_argument('image',type=Path);a=p.parse_args()
if not a.image.is_file() or a.image.is_symlink():p.error('Choose an ordinary .img file, not a device or symlink')
code=Path(os.environ.get('OVMF_CODE','/usr/share/OVMF/OVMF_CODE.fd'));var=Path(os.environ.get('OVMF_VARS','/usr/share/OVMF/OVMF_VARS.fd'))
if not code.is_file() or not var.is_file():p.error('Set OVMF_CODE and OVMF_VARS to installed x64 firmware files')
with tempfile.TemporaryDirectory(prefix='multios-vm-') as td:
 td=Path(td);image=td/'usb.img';shutil.copyfile(a.image,image);variables=td/'vars.fd';shutil.copyfile(var,variables)
 args=[os.environ.get('QEMU_UEFI','qemu-system-x86_64'),'-machine','q35','-m','256M','-drive',f'if=pflash,format=raw,readonly=on,file={code}','-drive',f'if=pflash,format=raw,file={variables}','-device','qemu-xhci,id=xhci','-drive',f'if=none,format=raw,id=stick,file={image}','-device','usb-storage,drive=stick,bus=xhci.0,bootindex=1','-device','usb-kbd,bus=xhci.0','-no-reboot']
 print('Temporary copy only. Changes are discarded when the VM exits.',flush=True)
 raise SystemExit(subprocess.call(args))
