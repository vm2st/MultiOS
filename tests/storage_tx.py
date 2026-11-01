#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — reproducible integrated transaction suite.
"""Requires dosfstools, mtools, exfatprogs; optional sanitizer executable.
Build with make test-storage-tx. Only disposable ordinary image files are used.
"""
from pathlib import Path
import subprocess,sys,os,struct,tempfile,random,hashlib,shutil,fcntl
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'tools'))
from mkimage import create
from prepare_journal import prepare
W=R/'build';logs=[];checks=0

def check(x,msg):
 global checks
 checks+=1
 if not x:raise AssertionError(msg)
def cmd(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=180)
 check(p.returncode==0,(a,p.stdout,p.stderr));return p.stdout

def shell(image,commands,errors=0):
 p=subprocess.run([str(W/'multios-host'),str(image),'--allow-write'],input=('\n'.join(commands+['exit'])+'\n').encode(),capture_output=True,timeout=180)
 logs.append(p.stdout.decode(errors='replace')+p.stderr.decode(errors='replace'))
 check(p.returncode==0 and p.stdout.count(b'Error:')==errors,p.stdout+p.stderr)
 return p.stdout

def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.digest()

def refusal(fn):
 try:fn()
 except (OSError,ValueError):return
 raise AssertionError('Expected preparation refusal')

def partition(image,out):
 with image.open('rb') as f:
  m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512)
  with out.open('wb') as g:
   left=n*512
   while left:
    b=f.read(min(left,1024*1024));check(bool(b),'short partition');g.write(b);left-=len(b)
 return start,n

def independent(image,kind,work,model=None):
 p=work/'independent.part';partition(image,p)
 text=cmd('fsck.fat' if kind=='fat' else 'fsck.exfat','-n',p)
 logs.append(text.decode(errors='replace'))
 if model is not None and kind=='fat':
  for name,content in sorted(model.items()):check(cmd('mtype','-i',p,'::'+name)==content,('independent FAT data',name))

for tool in ('mkfs.fat','mtype','fsck.fat','fsck.exfat'):check(shutil.which(tool),f'Missing {tool}')
with tempfile.TemporaryDirectory(prefix='storage-tx-',dir=W) as td:
 td=Path(td)
 for kind in ('fat','exfat'):
  source=td/(kind+'-source.img');image=td/(kind+'-prepared.img')
  if kind=='exfat':create(source)
  else:
   part=td/'fat.part'
   with part.open('wb') as f:f.truncate(64*1024*1024)
   cmd('mkfs.fat','-F','32','-s','1',part)
   m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,b'\0'*3,0x0c,b'\0'*3,2048,part.stat().st_size//512)
   with source.open('wb') as f:
    f.write(m);f.seek(2048*512)
    with part.open('rb') as g:shutil.copyfileobj(g,f)
  original=digest(source);prepare(source,image,1);check(digest(source)==original,'source modified')
  check(image.stat().st_size==source.stat().st_size+260*512,'wrong journal-tail size')
  shell(image,['unlock YES','write old.txt "original"','md empty','lock','fs'])
  independent(image,kind,td)
  for binary in ('storage-tx-test','storage-tx-sanitize'):
   text=cmd(W/binary,image).decode();logs.append(text);(W/(kind+'-'+binary+'.log')).write_text(text)
   check('PASS real-FS private transactions:' in text,text)
  # Seeded small-file model, independent checker after every batch.
  rng=random.Random(0x545831);model={};serial=0
  for batch in range(12):
   commands=['unlock YES']
   for i in range(8):
    serial+=1;dest=f'long generated file {serial}.txt';payload=f'batch{batch}-item{i}'
    op=rng.choice(['new','new','replace','append','copy','rename','delete'] if model else ['new'])
    if op=='new':commands.append(f'write "{dest}" "{payload}"');model[dest]=(payload+'\n').encode()
    else:
     src=rng.choice(sorted(model))
     if op=='replace':commands.append(f'write "{src}" "{payload}"');model[src]=(payload+'\n').encode()
     elif op=='append':commands.append(f'append "{src}" "{payload}"');model[src]+=(payload+'\n').encode()
     elif op=='copy':commands.append(f'copy "{src}" "{dest}"');model[dest]=model[src]
     elif op=='rename':commands.append(f'ren "{src}" "{dest}"');model[dest]=model.pop(src)
     else:commands.append(f'del "{src}"');del model[src]
   shell(image,commands+['lock']);independent(image,kind,td,model)
   for name,data in model.items():check(data in shell(image,[f'type "{name}"']),('VFS content',name))
  print(f'PASS {kind}: 96 seeded operations, 12 independent fsck checkpoints; crash/sanitizer logs saved',flush=True)
  # Real larger regular-file device: locate the journal through MBR, not EOF.
  larger=td/(kind+'-larger.img');shutil.copyfile(image,larger)
  with larger.open('ab') as f:f.write(bytes(8192*512))
  check(b'Private undo v1' in shell(larger,['fs','unlock YES','write bigger-device.txt "larger physical geometry"','lock']),'larger device RW')
  with larger.open('rb') as f:
   f.seek(image.stat().st_size);check(f.read()==bytes(8192*512),'unused physical tail modified')
  # Preparation safety: no source mutation, no overwrite, no devices, no aliases.
  refusal(lambda:prepare(source,image,1));check(digest(source)==original,'existing-output case modified source')
  refusal(lambda:prepare(source,td/'bad-index.img',5))
  refusal(lambda:prepare(image,td/'double.img',1))
  symlink=td/(kind+'-symlink');symlink.symlink_to(source);refusal(lambda:prepare(symlink,td/'alias.img',1));symlink.unlink()
  hardlink=td/(kind+'-hardlink');os.link(source,hardlink);refusal(lambda:prepare(source,td/'hard.img',1));hardlink.unlink()
  with source.open('r+b') as f:
   fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB);refusal(lambda:prepare(source,td/'locked.img',1))
  # Invalid MBR/geometry cases: preparation must refuse, never repair.
  raw=source.read_bytes()
  for case in ('overlap','gpt','no-free-slot','bad-geometry','ntfs','reserved-da'):
   bad=bytearray(raw)
   if case=='overlap':bad[462:478]=struct.pack('<B3sB3sII',0,b'\0'*3,7,b'\0'*3,2048,10)
   elif case=='gpt':bad[450]=0xee
   elif case=='no-free-slot':
    for i in range(1,4):bad[446+i*16:462+i*16]=struct.pack('<B3sB3sII',0,b'\0'*3,7,b'\0'*3,100*i,10)
   elif case=='bad-geometry':
    if kind=='exfat':struct.pack_into('<Q',bad,2048*512+72,1)
    else:struct.pack_into('<I',bad,2048*512+32,1)
   elif case=='ntfs':bad[2048*512+3:2048*512+11]=b'NTFS    ';bad[450]=7
   else:bad[450]=0xda
   malformed=td/(kind+'-'+case+'.img');malformed.write_bytes(bad);before=digest(malformed)
   refusal(lambda:prepare(malformed,td/(kind+'-'+case+'-out.img'),1));check(digest(malformed)==before,'refusal modified '+case);malformed.unlink()
  refusal(lambda:prepare(Path('/dev/null'),td/'device-out.img',1))
  check(digest(source)==original,'safety tests modified source')
 (W/'storage-tx-suite.log').write_text('\n'.join(logs))
(W/'storage-tx-suite-report.txt').write_text(f'PASS integrated transaction suite: {checks} Python assertions/commands plus separately counted C checks.\nFAT32 and exFAT: 96 seeded operations each, 12 fsck checkpoints each; ordinary + ASan/UBSan crash matrices.\nIndependent FAT file-content comparisons; exFAT fsck and VFS read comparisons.\nNOT full filesystem compatibility, NOT hardware certification, NTFS is not exercised by this FAT-only suite.\n')
print((W/'storage-tx-suite-report.txt').read_text())
