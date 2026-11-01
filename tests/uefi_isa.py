#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — supplemental baseline-x64 audit, not formal proof.
from pathlib import Path
import re, subprocess
root=Path(__file__).resolve().parents[1]
# Common SSE3/SSSE3/SSE4/AVX/BMI extensions. Baseline x86-64 includes SSE2.
post_sse2=set('addsubpd addsubps haddpd haddps hsubpd hsubps lddqu movddup movshdup movsldup pabsb pabsw pabsd palignr phaddw phaddd phaddsw phsubw phsubd phsubsw pmaddubsw pmulhrsw pshufb psignb psignw psignd blendpd blendps blendvpd blendvps dppd dpps extractps insertps movntdqa mpsadbw packusdw pblendvb pblendw pcmpeqq pcmpgtq pextrb pextrd pextrq phminposuw pinsrb pinsrd pinsrq pmaxsb pmaxsd pmaxuw pmaxud pminsb pminsd pminuw pminud pmovsxbd pmovsxbq pmovsxbw pmovsxdq pmovsxwd pmovsxwq pmovzxbd pmovzxbq pmovzxbw pmovzxdq pmovzxwd pmovzxwq pmuldq pmulld ptest roundpd roundps roundsd roundss crc32 pcmpestri pcmpestrm pcmpistri pcmpistrm popcnt lzcnt tzcnt movbe andn bextr bzhi mulx pdep pext rorx sarx shlx shrx rdrand rdseed xgetbv xsetbv xsave xrstor'.split())
def violations(text):
 result=[]
 for line in text.splitlines():
  m=re.match(r'\s*([0-9a-f]+):\s+([a-z][a-z0-9]*)\b',line)
  if m and (m[2] in post_sse2 or m[2].startswith(('v','aes','sha')) or re.search(r'\b[yz]mm\d+\b',line)):
   result.append(line.strip())
 return result
assert violations('16d4a: pminsd xmm0,xmm1'), 'regression detector missed original NTFS fault'
assert not violations('1000: pminsw xmm0,xmm1\n1004: movdqa xmm0,xmm1')
text=subprocess.check_output(['objdump','-d','--no-show-raw-insn','-Mintel',str(root/'build/multios-uefi.so')],text=True)
bad=violations(text)
assert not bad,'Unexpected optional x64 instruction:\n'+'\n'.join(bad[:20])
print('PASS supplemental UEFI ISA audit: no detected post-SSE2 instructions; original PMINSD regression detected by fixture. Runtime qemu64 test remains required.')
