#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — deterministic FAT32 model vs independent mtools/fsck.
from pathlib import Path
import hashlib,os,random,struct,subprocess
R=Path(__file__).resolve().parents[1];W=R/'build/fs12-model';W.mkdir(exist_ok=True)
host=os.environ.get('MULTIOS_HOST',str(R/'build/multios-host'));rng=random.Random(0x120C);image=W/'model.fat';logs=[];checks=0

def cmd(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=90)
 assert p.returncode==0,(a,p.stdout,p.stderr)
 return p.stdout

def shell(commands,errors=0):
 p=subprocess.run([host,str(image),'--allow-write'],input=('\n'.join(commands+['exit'])+'\n').encode(),capture_output=True,timeout=90)
 text=p.stdout.decode();logs.append(text)
 assert not p.returncode and text.count('Error:')==errors,(text,p.stderr)
 assert b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,p.stderr
 return text
with image.open('wb') as f:f.truncate(64*1024*1024)
cmd('mkfs.fat','-F','32','-s','1',image)
shell(['unlock YES','md DOCS','lock'])
model={};serial=0;operations=0
for batch in range(16):
 commands=['unlock YES']
 for j in range(12):
  choices=['new']+(['append','copy','rename','delete'] if model else [])
  kind=rng.choice(choices);serial+=1;dest=f'DOCS/file{serial}.txt'
  if kind=='new':
   payload=f'Seeded {serial} '+('X'*rng.randrange(1,120));commands.append(f'write {dest} "{payload}"');model[dest]=(payload+'\n').encode()
  else:
   src=rng.choice(sorted(model))
   if kind=='append':
    payload=f'Append{serial}';commands.append(f'append {src} "{payload}"');model[src]+=(payload+'\n').encode()
   elif kind=='copy':commands.append(f'copy {src} {dest}');model[dest]=model[src]
   elif kind=='rename':commands.append(f'ren {src} file{serial}.txt');model[dest]=model.pop(src)
   else:commands.append('del '+src);del model[src]
  operations+=1
 commands+=['lock','unlock YES','lock'];shell(commands)
 cmd('fsck.fat','-n',image);checks+=1
 for n,b in sorted(model.items()):assert cmd('mtype','-i',image,'::'+n)==b,n;checks+=1
 # Independently enumerate and compare complete directory, not just surviving known files.
 names={x.decode()[3:] for x in cmd('mdir','-b','-s','-i',image,'::DOCS/').splitlines() if not x.endswith(b'/')}
 assert names==set(model),(names,set(model));checks+=1
# A valid FAT32 media descriptor F0 is accepted; inconsistent reserved FAT[0] is denied.
b=bytearray(image.read_bytes());res=struct.unpack_from('<H',b,14)[0];fat=struct.unpack_from('<I',b,36)[0];backup=struct.unpack_from('<H',b,50)[0]
b[21]=0xf0;b[backup*512+21]=0xf0
for k in range(b[16]):struct.pack_into('<I',b,(res+k*fat)*512,0x0ffffff0)
image.write_bytes(b);shell(['unlock YES','write media.txt "media F0"','lock']);cmd('fsck.fat','-n',image);checks+=1
b=bytearray(image.read_bytes())
for k in range(b[16]):struct.pack_into('<I',b,(res+k*fat)*512,0x0ffffff8)
image.write_bytes(b);before=hashlib.sha256(b).digest();shell(['unlock YES','touch denied'],2)
assert hashlib.sha256(image.read_bytes()).digest()==before;checks+=1
(W/'console.log').write_text('\n'.join(logs))
report=f'PASS: seed 0x120C; {operations} modelled create/append/copy/rename/delete operations; {checks} independent comparisons/checkpoints; 16 fsck checkpoints; valid F0 and mismatched FAT media guard.\nNo claim of crash consistency or hardware certification.\n'
(R/'build/dev12-fat32-model-report.txt').write_text(report);print(report)
