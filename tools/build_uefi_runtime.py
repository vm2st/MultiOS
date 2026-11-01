#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — reproducible baseline-x64 GNU-EFI runtime.
from pathlib import Path
import argparse,tarfile,tempfile,subprocess,shutil
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--cc',default='cc');a=p.parse_args()
archive=root/'third_party/gnu-efi/gnu-efi-3.0.11.tar.gz'
sdk=root/'build/gnu-efi-sdk'
with tempfile.TemporaryDirectory(prefix='gnuefi-build-',dir=root/'build') as td:
 t=Path(td)
 with tarfile.open(archive) as z:
  for m in z.getmembers():
   if m.name.startswith('/') or '..' in Path(m.name).parts or m.issym() or m.islnk():
    raise ValueError('Unsafe GNU-EFI archive member')
  z.extractall(t)
 source=t/'gnu-efi-3.0.11'
 # Preserve upstream's PIC/wchar/ABI flags; overriding CFLAGS would drop them.
 compiler=a.cc+' -march=x86-64 -mtune=generic -fcf-protection=none -ffile-prefix-map='+str(source)+'=gnu-efi-3.0.11'
 subprocess.run(['make','-C',str(source),'ARCH=x86_64','CC='+compiler,'lib','gnuefi'],check=True)
 out=t/'sdk';(out/'lib/gnuefi').mkdir(parents=True)
 shutil.copytree(source/'inc',out/'include/efi')
 shutil.copy2(source/'x86_64/lib/libefi.a',out/'lib/libefi.a')
 for name in ('libgnuefi.a','crt0-efi-x86_64.o','elf_x86_64_efi.lds'):
  src=source/'x86_64/gnuefi'/name
  if not src.exists():src=source/'gnuefi'/name
  shutil.copy2(src,out/'lib/gnuefi'/name)
 (out/'BUILD.txt').write_text('GNU-EFI 3.0.11; '+compiler+'; upstream build flags retained.\n')
 if sdk.exists():shutil.rmtree(sdk)
 shutil.copytree(out,sdk)
(root/'build/gnuefi.stamp').write_text('GNU-EFI 3.0.11 baseline x86-64; compiler='+a.cc+'\n')
print('PASS bundled GNU-EFI SDK built for baseline x86-64, not distribution x86-64-v2.')
