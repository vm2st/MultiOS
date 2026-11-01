#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — disposable corruption fixtures, no repair.
from pathlib import Path
import sys,subprocess,tempfile,shutil,struct,hashlib,os
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
from mkimage import entry,setcheck,checksum
exe=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-host'))
logs=[];checks=0

def mutate(path,kind):
 with path.open('r+b') as f:
  def read(off,n):f.seek(off);return bytearray(f.read(n))
  def write(off,b):f.seek(off);f.write(b)
  base=2048*512;b=read(base,512);fat,heap,nc,root=struct.unpack_from('<I',b,80)[0],struct.unpack_from('<I',b,88)[0],struct.unpack_from('<I',b,92)[0],struct.unpack_from('<I',b,96)[0]
  spc=1<<b[109];cs=spc*512
  def cl(c):return base+(heap+(c-2)*spc)*512
  rp=cl(root);r=read(rp,cs*8);bm=struct.unpack_from('<I',r,52)[0];up=struct.unpack_from('<I',r,84)[0];data=struct.unpack_from('<I',r,148)[0]
  def bit(c,value):
   pos=cl(bm)+(c-2)//8;v=read(pos,1)[0];mask=1<<((c-2)&7);write(pos,bytes([(v|mask) if value else (v&~mask)]))
  def fatset(c,v):write(base+fat*512+c*4,struct.pack('<I',v))
  def file(name,first,size,flags=1,attr=32,pos=96):write(rp+pos,entry(name,first,size,flags,attr))
  def recheck(start):
   region=read(base+start*512,11*512);write(base+(start+11)*512,struct.pack('<I',checksum(region,(106,107,112)))*128)
  if kind=='partition-short':write(446+12,struct.pack('<I',struct.unpack_from('<I',b,72)[0]-1))
  elif kind=='backup-checksum':write(base+23*512,b'\x01\x02\x03\x04')
  elif kind=='backup-geometry':write(base+12*512+88,struct.pack('<I',heap+8));recheck(12)
  elif kind=='reserved-field':
   for start in (0,12):write(base+start*512+11,b'\x01');recheck(start)
  elif kind=='percentage':write(base+112,b'\x65')
  elif kind.startswith('free-'):
   bit({'free-root':root,'free-bitmap':bm,'free-upcase':up,'free-file':data}[kind],False)
  elif kind=='orphan':bit(40,True);fatset(40,0xffffffff)
  elif kind=='file-root-crosslink':file('WELCOME.TXT',root,cs*8)
  elif kind=='two-file-crosslink':file('ALIAS.TXT',data,64,pos=192)
  elif kind=='chain-cycle':file('WELCOME.TXT',data,cs*2);fatset(data,data)
  elif kind=='chain-short':file('WELCOME.TXT',data,cs*2)
  elif kind=='chain-long':bit(40,True);fatset(40,0xffffffff);fatset(data,40)
  elif kind=='empty-with-cluster':file('WELCOME.TXT',data,0)
  elif kind=='zero-directory':file('BADDIR',0,0,attr=16)
  elif kind=='unaligned-directory':file('BADDIR',data,1,attr=16)
  elif kind=='directory-cycle':file('BADDIR',root,cs*8,attr=16)
  elif kind=='unknown-benign':write(rp+192,bytes([0xa0])+bytes(31))
  elif kind=='extra-secondary':
   e=entry('WELCOME.TXT',data,64);e+=bytes([0xe1])+bytes(31);e[1]=3;e[2:4]=struct.pack('<H',setcheck(e));write(rp+96,e)
  elif kind=='huge-directory':file('BADDIR',data,0xfffff000,3,16)
  elif kind=='root-fat-cycle':fatset(root,root)
  else:raise AssertionError(kind)

cases=['partition-short','backup-checksum','backup-geometry','reserved-field','percentage',
 'free-root','free-bitmap','free-upcase','free-file','file-root-crosslink','two-file-crosslink',
 'chain-cycle','chain-short','chain-long','empty-with-cluster','zero-directory','unaligned-directory',
 'directory-cycle','unknown-benign','extra-secondary','huge-directory','root-fat-cycle']
with tempfile.TemporaryDirectory() as td:
 img=Path(td)/'bad.img'
 for kind in cases:
  shutil.copyfile(ROOT/'build/multios.img',img);mutate(img,kind)
  before=hashlib.sha256(img.read_bytes()).digest()
  p=subprocess.run([exe,str(img),'--allow-write'],input='unlock YES\ntouch PROBE.TXT\nexit\n',text=True,capture_output=True,timeout=20)
  logs.append(kind+'\n'+p.stdout+p.stderr)
  assert p.returncode in (0,1),logs[-1]
  assert 'EXPERIMENTAL WRITE ENABLED' not in p.stdout and 'Error:' in p.stdout,logs[-1]
  assert hashlib.sha256(img.read_bytes()).digest()==before,kind+' changed image'
  assert 'Sanitizer' not in p.stderr and 'runtime error:' not in p.stderr,p.stderr
  checks+=4
 # Unreferenced allocated reservations must never be reclaimed or reused.
 shutil.copyfile(ROOT/'build/multios.img',img);mutate(img,'orphan')
 p=subprocess.run([exe,str(img),'--allow-write'],input='unlock YES\nwrite KEEP.TXT "kept"\nexit\n',text=True,capture_output=True,timeout=20)
 assert p.returncode==0 and 'EXPERIMENTAL WRITE ENABLED' in p.stdout and 'Error:' not in p.stdout,p.stdout
 with img.open('rb') as f:
  f.seek(2048*512);boot=f.read(512);heap=struct.unpack_from('<I',boot,88)[0];spc=1<<boot[109]
  f.seek((2048+heap)*512+52);bm=struct.unpack('<I',f.read(4))[0]
  f.seek((2048+heap+(bm-2)*spc)*512+(40-2)//8);assert f.read(1)[0]&(1<<((40-2)&7))
 checks+=2
 # Valid disposable volume must still permit a write, clean re-audit, and re-open.
 shutil.copyfile(ROOT/'build/multios.img',img)
 p=subprocess.run([exe,str(img),'--allow-write'],input='unlock YES\nwrite SAFE.TXT "safe"\nlock\nunlock YES\ntype SAFE.TXT\nexit\n',text=True,capture_output=True,timeout=20)
 assert p.returncode==0 and p.stdout.count('EXPERIMENTAL WRITE ENABLED')==2 and '\nsafe\n' in p.stdout and 'Error:' not in p.stdout,p.stdout
 checks+=1
 part=Path(td)/'data.exfat'
 with img.open('rb') as f:f.seek(2048*512);part.write_bytes(f.read())
 q=subprocess.run(['fsck.exfat','-n',str(part)],text=True,capture_output=True);assert q.returncode==0,q.stdout+q.stderr;checks+=1
report=f'{checks} adverse-volume assertions PASS: {len(cases)} corrupted fixtures deny mount/write and stay byte-identical; reserved allocated clusters preserved; clean WRITE/LOCK/re-UNLOCK and independent fsck pass. Bounded consistency preflight, not full fsck or power-loss certification.\n'
(ROOT/'build/dev10-adverse-volume-report.txt').write_text(report)
(ROOT/'build/dev10-adverse-volume-logs.txt').write_text('\n'.join(logs));print(report)
