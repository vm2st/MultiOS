#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — supplemental instruction audit, NOT a formal verifier.
from pathlib import Path
import re,subprocess
ROOT=Path(__file__).resolve().parents[1]
text=subprocess.check_output(['objdump','-d','-Mintel',str(ROOT/'build/kernel.elf')],text=True)
# Ban common accidental post-i386 integer instructions and FP/SIMD in BIOS kernel.
patterns=[r'\b(?:xmm|ymm|zmm|mm[0-7]|st\([0-7]\))\b',
          r'\b(?:cmov\w*|finit|fninit|fld\w*|fst\w*|fadd\w*|fmul\w*|fdiv\w*|fxsave\w*|fxrstor\w*|xsave\w*|xrstor\w*|rdtsc|rdtscp|rdrand|rdseed|bswap|cmpxchg\w*|xadd|movbe|popcnt|lzcnt|tzcnt|syscall|sysenter)\b']
# Match instruction bytes at their START, not a call/jump displacement that
# merely contains 0f 1f. The old whole-line search produced false positives.
patterns.append(r'(?m)^\s*[0-9a-f]+:\s+(?:66\s+)*0f\s+1f\b')
for p in patterns:assert not re.search(p,text),('unexpected optional ISA',p)
current=''
for line in text.splitlines():
    label=re.match(r'^[0-9a-f]+ <([^>]+)>:',line)
    if label:current=label.group(1)
    if re.search(r'\s+cpuid\s*$',line):
        assert current.startswith(('leaf','multi_x86_cpu_probe')),('unguarded CPUID location',current)
disabled=ROOT/'build/pc-cpu-disabled.o'
if disabled.exists():
    dump=subprocess.check_output(['objdump','-d',str(disabled)],text=True)
    assert not re.search(r'\s+cpuid\s*$',dump,re.M),'forced fallback object still executes CPUID'
# Guard itself is 8086-safe: inspect actual bytes, not just the .code16 directive.
boot=subprocess.check_output(['objdump','-D','-mi8086',str(ROOT/'build/boot.o')],text=True)
pre=boot.split('<start>:',1)[1].split('sti',1)[0]
for line in pre.splitlines():
    m=re.match(r'\s*[0-9a-f]+:\s+((?:[0-9a-f]{2}\s+)+)',line)
    if m:assert m.group(1).split()[0] not in ('0f','66','67'),line
assert 'inb $0x92' not in (ROOT/'platform/pc-bios/boot.S').read_text()
print('Supplemental ISA audit passed: BIOS kernel has no detected FP/SIMD/CMOV/TSC dependency; early CPU guard is 16-bit. CPUID appears only in guarded BSP probe.')
