#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — emulator configurations, NOT physical CPU certification.
from pathlib import Path
import os,shutil,subprocess
from qemu_smoke import run_qemu,ROOT
models=['486','pentium','pentium2','pentium3','athlon','n270','coreduo','core2duo','qemu32','qemu64','phenom','Conroe','Nehalem','Westmere','SandyBridge','IvyBridge','Haswell','Broadwell','Skylake-Client','EPYC',
        'qemu32,vendor=VM2STUDIOS00','486,-fpu,-mmx,-sse,-sse2,-cmov','qemu64,-fpu,-mmx,-sse,-sse2,-cmov']
work=ROOT/'build/cpu-matrix';work.mkdir(exist_ok=True)
report=['MultiPulti 0.12-dev (unpublished) | vm2 studios | t.me/v2vproj',
        'Same BIOS i386 kernel, PC/IDE/SeaBIOS under QEMU TCG. Model names are not hardware certification.']
for i,model in enumerate(models):
    image=work/'work.img';shutil.copyfile(ROOT/'build/multios.img',image)
    log=run_qemu(image,['cpu','ver','unlock YES','write cpu.txt "CPU matrix data"','type cpu.txt','exit'],model)
    assert 'Required ISA: i386 integer baseline' in log and 'CPU matrix data' in log and 'Error:' not in log,log
    if '-fpu' in model:
        featureline=log.split('Advertised features (not enabled/used by this report):',1)[1].split('\n',1)[0]
        assert not any(x in featureline.split() for x in ('FPU','MMX','SSE','SSE2','CMOV')),featureline
    (work/f'{i:02d}.log').write_text(log)
    report.append('PASS '+model)
    print(report[-1],flush=True)
image=work/'no-cpuid.img';shutil.copyfile(ROOT/'build/cpu-disabled.img',image)
log=run_qemu(image,['cpu','dir','unlock YES','write fallback.txt "No CPUID path"','type fallback.txt','exit'],'486')
assert 'unavailable or disabled; baseline path active' in log and 'No CPUID path' in log and 'Error:' not in log,log
(work/'no-cpuid.log').write_text(log)
report.append('PASS forced-no-CPUID build on emulated 486 (NOT a real 386/no-CPUID processor test)')
report.append(f'{len(models)} same-binary CPU configurations + one forced fallback build passed. No 8086/186/286 or physical 386 tested/supported by this matrix.')
(ROOT/'build/cpu-matrix-report.txt').write_text('\n'.join(report)+'\n')
print(report[-1])
