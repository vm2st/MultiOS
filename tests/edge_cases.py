#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Adversarial/edge fixtures: bounded to disposable images, never raw devices."""
import pathlib,sys,subprocess,tempfile,shutil,struct,zlib,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mkimage import entry,setcheck
base=2048*512;checks=0
reports=[]
def check(ok,msg):
 global checks
 checks+=1
 if not ok:raise AssertionError(msg)
def run(img,cmds,errors=0):
 exe=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-host'))
 p=subprocess.run([exe,str(img),'--allow-write'],input='\n'.join(cmds+['exit'])+'\n',text=True,capture_output=True,timeout=30)
 reports.append(p.stdout+p.stderr);check(p.returncode==0,p.stdout+p.stderr);check(p.stdout.count('Error:')==errors,p.stdout)
 return p.stdout
def fsck(img):
 if not shutil.which('fsck.exfat'):return
 temp=img.with_suffix('.exfat')
 with img.open('rb') as f:f.seek(base);temp.write_bytes(f.read())
 p=subprocess.run(['fsck.exfat','-n',str(temp)],capture_output=True,text=True)
 reports.append(p.stdout+p.stderr);check(p.returncode==0,p.stdout+p.stderr)
def fixture(img,kind):
 with img.open('r+b') as f:
  f.seek(base);b=f.read(512);fat=struct.unpack_from('<I',b,80)[0];heap=struct.unpack_from('<I',b,88)[0];spc=1<<b[109];cs=spc*512
  root=struct.unpack_from('<I',b,96)[0];rootpos=base+(heap+(root-2)*spc)*512
  f.seek(rootpos);rootdata=f.read(cs*8);bm=struct.unpack_from('<I',rootdata,52)[0]
  bmpos=base+(heap+(bm-2)*spc)*512
  def setfat(c,n):f.seek(base+fat*512+c*4);f.write(struct.pack('<I',n))
  def setbit(c):
   pos=bmpos+(c-2)//8;f.seek(pos);n=f.read(1)[0]|(1<<((c-2)%8));f.seek(pos);f.write(bytes([n]))
  clusters=[40,44,48] if kind=='fragmented' else [40,41,42]
  data=bytes((i*37+19)%256 for i in range(cs*3-17));valid=cs+3 if kind=='vdl' else len(data)
  for k,c in enumerate(clusters):
   setbit(c);setfat(c,clusters[k+1] if k<2 else 0xffffffff)
   f.seek(base+(heap+(c-2)*spc)*512);f.write(data[k*cs:(k+1)*cs])
  name='EDGE.BIN';en=entry(name,clusters[0],len(data),3 if kind=='contiguous' else 1)
  if kind=='vdl':
   en[40:48]=struct.pack('<Q',valid);en[2:4]=struct.pack('<H',setcheck(en))
  f.seek(rootpos+192);f.write(en);f.write(b'\0'*32)
  return data[:valid]+bytes(len(data)-valid)
with tempfile.TemporaryDirectory() as td:
 td=pathlib.Path(td)
 for kind in ['fragmented','contiguous','vdl']:
  img=td/f'{kind}.img';shutil.copyfile(ROOT/'build/multios.img',img);data=fixture(img,kind)
  s=run(img,['crc32 EDGE.BIN','unlock YES','copy EDGE.BIN COPY.BIN','cmp EDGE.BIN COPY.BIN','crc32 COPY.BIN','append EDGE.BIN "tail"','crc32 EDGE.BIN','del COPY.BIN','del EDGE.BIN'])
  check(s.count(f'{zlib.crc32(data):08X}')==2,'bad CRC '+kind)
  check(f'{zlib.crc32(data+b"tail\n"):08X}' in s,'bad append '+kind);check('Identical' in s,'bad copy '+kind);fsck(img)
 # Directory is fixed-size and must fail gracefully without dirtying disk.
 img=td/'full.img';shutil.copyfile(ROOT/'build/multios.img',img)
 # 4096-byte child directory: 128 slots => 42 entries of 3 slots.
 commands=['unlock YES','md FULL']+[f'touch FULL/F{i}' for i in range(42)]+['touch FULL/OVERFLOW','stat FULL/F41','df']
 s=run(img,commands,1);check('disk or directory full' in s and 'Dirty: 0' in s,'full directory should fail before mutation');fsck(img)
 # Same-directory case-only rename and prohibition of renaming/deleting CWD ancestors.
 img=td/'paths.img';shutil.copyfile(ROOT/'build/multios.img',img)
 s=run(img,['unlock YES','md A','md A/B','write A/B/T "ok"','cd A/B','ren /a x','rd /a','cd ../../..','pwd','ren A a','type /a/b/t','write "space name.txt" "text with spaces"','type "space name.txt"','ren "space name.txt" "new name.txt"','hex "new name.txt"','history'],2)
 check('text with spaces' in s,'quoted parsing');fsck(img)
 # File-entry checksum corruption must not be accepted.
 img=td/'entry-corrupt.img';shutil.copyfile(ROOT/'build/multios.img',img)
 with img.open('r+b') as f:f.seek(base+(2048)*512+98);f.write(b'\0\0')
 s=run(img,['dir'],1);check('invalid/corrupt' in s,'bad entry checksum accepted')
 # Too-long line, invalid quote, name rejection are non-destructive.
 img=td/'syntax.img';shutil.copyfile(ROOT/'build/multios.img',img)
 before=img.read_bytes();s=run(img,['write x "'+('a'*600)+'"','write x "unterminated','unlock YES','touch "bad:name"','dir'],3)
 check(img.read_bytes()==before,'parser/name error modified volume')
(ROOT/'build/edge-report.txt').write_text(f'{checks} edge assertions passed\n\n'+'\n\n'.join(reports))
print(f'{checks} edge assertions passed')
