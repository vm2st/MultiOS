#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Real UEFI boot tests for runtime module selections and strict config errors."""
from pathlib import Path
from uefi_smoke import boot, ROOT
base=(ROOT/'profiles/BOOT.CFG').read_text()
log,seconds,_=boot(['modules','platform','unlock YES','dir','poweroff'],ROOT/'build/module-ro-test',
    config=base.replace('block=uefi-file\n','block=uefi-file-ro\n').replace('osk=auto','osk=off'),osk=False)
assert 'block: uefi-file-ro' in log and 'Block write capability: no' in log and 'Error:' in log,log
(ROOT/'build/module-ro-test.log').write_text(log)
log,seconds,_=boot(['modules','osk on','dir','poweroff'],ROOT/'build/module-plain-test',
    config=base.replace('output=uefi-text','output=uefi-plain').replace('osk=auto','osk=off'),osk=False)
assert 'output: uefi-plain' in log and 'Error:' in log,log
(ROOT/'build/module-plain-test.log').write_text(log)
for label,config,marker in [
    ('unknown',base.replace('input=uefi-keys','input=missing-driver'),'module start failed: input'),
    ('wrong-kind',base.replace('input=uefi-keys','input=uefi-text'),'module start failed: input'),
    ('bad-option',base+'input.options=invalid\n','module start failed: uefi-keys'),
    ('bad-config',base+'unknown=abc\n','invalid MULTIOS'),
    ('duplicate',base+'input=uefi-keys\n','invalid MULTIOS'),
    ('osk-no-renderer',base.replace('output=uefi-text','output=uefi-plain').replace('osk=auto','osk=on'),'output has no OSK renderer'),
]:
    log,_,_=boot([],ROOT/('build/module-error-'+label),config=config,osk=False,failure=marker)
    assert 'HELP for commands.' not in log,log
    (ROOT/('build/module-error-'+label+'.log')).write_text(log)
print('UEFI runtime selection passed: forced RO, plain output, 6 fail-closed boots.')
