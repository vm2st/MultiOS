#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — malicious UTF-16 LFNs/hash/padding; no writes on failed audit.
from pathlib import Path
import subprocess,struct,hashlib,os,shutil
R=Path(__file__).resolve().parents[1];W=R/'build/unicode-metadata';W.mkdir(exist_ok=True);host=os.environ.get('MULTIOS_HOST',str(R/'build/multios-host'));checks=0;logs=[]
os.environ['LC_ALL']='C.utf8'
def ck(v,msg):
 global checks
 checks+=1
 if not v:raise AssertionError(msg)
def run(p,commands):
 r=subprocess.run([host,str(p),'--allow-write'],input=('\n'.join(commands+['exit'])+'\n').encode(),capture_output=True,timeout=90)
 logs.append(r.stdout.decode('utf-8','replace')+r.stderr.decode('utf-8','replace'));ck(r.returncode==0 and b'AddressSanitizer' not in r.stderr and b'runtime error:' not in r.stderr,(r.stdout,r.stderr));return r.stdout
fat=W/'source.fat'
with fat.open('wb') as f:f.truncate(64*1024*1024)
subprocess.run(['mkfs.fat','-F','32','-s','1',str(fat)],check=True,stdout=subprocess.DEVNULL)
txt=W/'source.txt';txt.write_text('SAFE\n');subprocess.run(['mcopy','-i',str(fat),str(txt),'::Пример😀.txt'],check=True)
b=fat.read_bytes();reserved=struct.unpack_from('<H',b,14)[0];fsz=struct.unpack_from('<I',b,36)[0];heap=(reserved+b[16]*fsz)*512;root=heap+(struct.unpack_from('<I',b,44)[0]-2)*b[13]*512
lfn=next(i for i in range(root,root+512,32) if b[i+11]==15)
for label,off,data in [('high-surrogate',lfn+1,b'\x00\xd8'),('lone-low',lfn+1,b'\x00\xdc'),('embedded-control',lfn+1,b'\x01\x00'),('bad-lfn-checksum',lfn+13,bytes([b[lfn+13]^1]))]:
 bad=bytearray(b);bad[off:off+len(data)]=data;p=W/(label+'.fat');p.write_bytes(bad);before=hashlib.sha256(bad).digest();s=run(p,['dir','unlock YES','touch forbidden'])
 ck(b'EXPERIMENTAL WRITE ENABLED' not in s and s.count(b'Error:')>=2,label);ck(hashlib.sha256(p.read_bytes()).digest()==before,label+' source unchanged')
# A valid UTF-16 filename outside the current 255-byte UTF-8 VFS limit is listed,
# NOT truncated into a different addressable name; ASCII SFN remains readable.
long=W/'long.fat';shutil.copyfile(fat,long);subprocess.run(['mcopy','-i',str(long),str(txt),'::'+('Ж'*180)+'.txt'],check=True)
s=run(long,['dir','unlock YES']);ck(b'name exceeds UTF-8 path limit' in s and b'EXPERIMENTAL WRITE ENABLED' not in s,'unaddressable UTF-8 length must block write preflight')
# exFAT invalid Unicode with valid entry checksum, or mismatched name hash.
ex=W/'exfat.img';shutil.copyfile(R/'build/multios.img',ex);s=run(ex,['unlock YES','write Пример😀.txt "SAFE"'])
ck(b'Error:' not in s,'exFAT baseline')
b=bytearray(ex.read_bytes());base=2048*512;spc=1<<b[base+109];heap=base+struct.unpack_from('<I',b,base+88)[0]*512;root=heap+(struct.unpack_from('<I',b,base+96)[0]-2)*spc*512
primary=None
for p in range(root,root+spc*512,32):
 if b[p]==0x85 and b[p+35] and struct.unpack_from('<H',b,p+66)[0]==0x41f:primary=p;break
ck(primary is not None,'exFAT Unicode fixture')
def setcheck(a):
 n=(a[primary+1]+1)*32;x=0
 for i in range(n):
  if i not in (2,3):x=(((x>>1)|(x<<15))+a[primary+i])&65535
 struct.pack_into('<H',a,primary+2,x)
for label,off,value in [('high-surrogate',primary+66,0xd800),('lone-low',primary+66,0xdc00),('bad-namehash',primary+36,0),('control',primary+66,1)]:
 bad=bytearray(b);struct.pack_into('<H',bad,off,value);setcheck(bad);p=W/(label+'.exfat');p.write_bytes(bad);before=hashlib.sha256(bad).digest();s=run(p,['dir','unlock YES','touch forbidden'])
 ck(b'EXPERIMENTAL WRITE ENABLED' not in s and s.count(b'Error:')>=2,label);ck(hashlib.sha256(p.read_bytes()).digest()==before,label+' exFAT unchanged')
tag='san-' if os.environ.get('MULTIOS_HOST') else '';report=f'PASS {checks} Unicode hostile-metadata assertions: isolated surrogate/control/checksum/hash corruptions; valid but overlong UTF-8 display label; failed audits never modify sources.\n';(R/'build'/('unicode-'+tag+'metadata-report.txt')).write_text(report);(R/'build'/('unicode-'+tag+'metadata.log')).write_text('\n'.join(logs));print(report)
