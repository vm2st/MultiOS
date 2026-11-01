#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Build a different source/registry profile, boot it, restore default and boot it.
No make clean between switches: catches stale profile object/link problems.
"""
from uefi_smoke import boot, ROOT
import subprocess
with (ROOT/'build/profile-switch-build.log').open('w') as report:
    try:
        subprocess.run(['make','usb','UEFI_PROFILE=profiles/uefi-minimal.mk'],cwd=ROOT,stdout=report,stderr=subprocess.STDOUT,check=True)
        log,_,_=boot(['modules','platform','poweroff'],ROOT/'build/profile-minimal-test',osk=False)
        assert 'output: uefi-plain' in log and 'block: uefi-file-ro' in log and 'Block write capability: no' in log,log
        symbols=subprocess.check_output(['nm','build/multios-uefi.so'],cwd=ROOT,text=True)
        assert ' multi_uefi_text' not in symbols
        (ROOT/'build/profile-minimal-test.log').write_text(log)
    finally:
        subprocess.run(['make','usb'],cwd=ROOT,stdout=report,stderr=subprocess.STDOUT,check=True)
    log,_,_=boot(['modules','poweroff'],ROOT/'build/profile-restored-test')
    assert 'output: uefi-text' in log and 'block: uefi-file -' in log,log
    (ROOT/'build/profile-restored-test.log').write_text(log)
print('Source/registry profile switch + boot + restoration passed without make clean.')
