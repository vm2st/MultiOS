#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — supplemental structural audit, not a security proof.
from pathlib import Path
import subprocess,re
ROOT=Path(__file__).resolve().parents[1]
b=ROOT/'build'
symbols={}
for line in subprocess.check_output(['nm','-S','--defined-only',str(b/'kernel.elf')],text=True).splitlines():
 parts=line.split()
 if len(parts)==4:symbols[parts[3]]=(int(parts[0],16),int(parts[1],16),parts[2])
 elif len(parts)==3:symbols[parts[2]]=(int(parts[0],16),0,parts[1])
assert not subprocess.check_output(['nm','-u',str(b/'kernel.elf')],text=True).strip()
for name,size in [('idt',2048),('gdt',40),('normal_tss',104),('df_tss',104),('df_stack',8192),('__stack_chk_guard',4)]:
 assert symbols[name][1]==size,(name,symbols.get(name))
assert symbols['df_stack'][0]%16==0
assert len([n for n in symbols if n.startswith('multi_fault_stub_')])==256
assert 'multi_stack_fault_test' not in symbols,'TEST function in production'
assert (b/'kernel.bin').stat().st_size<=122880
for obj in ('shell.o','exfat.o'):
 text=subprocess.check_output(['objdump','-dr',str(b/obj)],text=True)
 assert '__stack_chk_guard' in text and '__stack_chk_fail' in text,obj
text=subprocess.check_output(['objdump','-dr',str(b/'exceptions.o')],text=True)
assert not re.search(r'R_386_PC32\s+__stack_chk_fail',text),'panic path recursively protected'
subprocess.run(['python3',str(ROOT/'tests/cpu_isa.py')],check=True)
report='Structural audit PASS: 256 stubs; IDT/TSS/GDT/8KiB emergency stack sizes; no unresolved symbols; no fault-test function in production; compiler canaries in shell/exFAT; panic path uninstrumented; loader size and supplemental i386 ISA audit. Not a security proof.\n'
(b/'kernel-guard-audit-report.txt').write_text(report);print(report)
