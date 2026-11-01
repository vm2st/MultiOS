#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Static boundary check, not a substitute for compiling on another ISA."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
files=[*root.glob('lib/*.c'),*root.glob('lib/*.h'),*root.glob('core/*.c'),*root.glob('include/multios/*.h')]
patterns=[r'\b__asm__\b',r'\basm\s*\(',r'\b0x[bB]8000\b',r'\b(?:pread|pwrite|fsync|inb|outb|inw|outw)\s*\(',r'#\s*include\s*[<"](?:stdio|unistd|fcntl|sys/)']
for f in files:
 text=f.read_text()
 # NTFS-3G's HAL bridge uses POSIX-shaped types/constants in declarations.
 # These two includes do NOT grant permission for actual POSIX I/O calls.
 if f.name=='ntfs3g_adapter.c':
  text=re.sub(r'^#include <(?:fcntl.h|sys/stat.h)>$', '', text, flags=re.M)
 for p in patterns:
  assert not re.search(p,text),f'platform dependency {p}: {f}'
print(f'Portable boundary check passed: {len(files)} files; no CPU I/O, ASM or POSIX calls in core.')
