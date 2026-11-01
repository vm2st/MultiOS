#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — no FUSE mount; independent ntfscat + metadata tools.
from pathlib import Path
import subprocess,tempfile,sys,struct,hashlib,shutil
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));from prepare_journal import prepare
logs=[];checks=0
def ck(x,msg):
 global checks
 checks+=1
 if not x:raise AssertionError(msg)
def cmd(*a):
 p=subprocess.run(list(map(str,a)),capture_output=True,timeout=180);ck(p.returncode==0,(a,p.stdout[-2000:],p.stderr[-2000:]));logs.append((p.stdout+p.stderr).decode(errors='replace'));return p.stdout
def shell(image,commands,errors=0):
 p=subprocess.run([str(r/'build/multios-host'),str(image),'--allow-write'],input=('\n'.join(commands+['exit'])+'\n').encode(),capture_output=True,timeout=180)
 ck((p.returncode in (0,1) if errors else p.returncode==0) and p.stdout.count(b'Error:')==errors,(p.stdout[-4000:],p.stderr[-2000:]));logs.append(p.stdout.decode(errors='replace'));return p.stdout
def wrap(part,source):
 n=part.stat().st_size//512;m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,b'\0'*3,7,b'\0'*3,2048,n)
 with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
def extract(image,part):
 with image.open('rb') as f:m=f.read(512);start,n=struct.unpack_from('<II',m,454);f.seek(start*512);part.write_bytes(f.read(n*512))
with tempfile.TemporaryDirectory(prefix='ntfs-upstream-',dir=r/'build') as d:
 d=Path(d);part=d/'ntfs.part';source=d/'source.img';image=d/'test.img'
 with part.open('wb') as f:f.truncate(64*1024*1024)
 cmd('mkntfs','-F','-Q','-s','512','-c','4096',part);wrap(part,source);before=hashlib.sha256(source.read_bytes()).digest();prepare(source,image,1,experimental_ntfs=True)
 ck(hashlib.sha256(source.read_bytes()).digest()==before,'source altered')
 print(cmd(r/'build/upstream-growth',image).decode().strip(),flush=True);extract(image,part)
 ck(cmd('ntfscat',part,'/large.bin')==bytes(range(256))*640+b'TAIL','independent NTFS content')
 ck(cmd('ntfscat',part,'/тест.txt')==b'cp866','independent UTF8 name')
 listing=cmd('ntfsls',part).decode(errors='replace');ck(listing.count('long filename for directory growth number')==120,'independent directory listing')
 cmd('ntfsinfo','-m',part);cmd('ntfsfix','-n',part)
 # Rename/delete plus Unicode, case-insensitive lookup, and preserved ADS-unaware bytes.
 shell(image,['unlock YES','md EXTRA','write EXTRA/alpha.txt "ONE"','append extra/ALPHA.TXT "TWO"','copy EXTRA/alpha.txt EXTRA/copied.txt','ren EXTRA/copied.txt result.txt','del EXTRA/alpha.txt','lock'])
 extract(image,part);ck(cmd('ntfscat',part,'/EXTRA/result.txt')==b'ONE\nTWO\n','independent rename/append/copy')
 # Protect system metadata and reject unsafe overwrite/recursive delete.
 shell(image,['unlock YES','write /$MFT "DENY"'],errors=1)
 shell(image,['unlock YES','write /$Extend/guard.txt "DENY"'],errors=1)
 shell(image,['unlock YES','rd EXTRA'],errors=1)
 extract(image,part);ck(cmd('ntfscat',part,'/EXTRA/result.txt')==b'ONE\nTWO\n','guard damaged original')
 # Windows hibernation: source image with hiberfil.sys; unlock must not write.
 raw=d/'hiber.part';shutil.copyfile(d/'ntfs.part',raw);hiber=d/'hiber.dat';hiber.write_bytes(b'hibr'+bytes(4092));cmd('ntfscp','-f',raw,hiber,'/hiberfil.sys')
 hs=d/'hiber-source.img';hi=d/'hiber-test.img';wrap(raw,hs);prepare(hs,hi,1,experimental_ntfs=True);h=hashlib.sha256(hi.read_bytes()).digest()
 shell(hi,['unlock YES'],errors=1);ck(hashlib.sha256(hi.read_bytes()).digest()==h,'hibernated image modified')
 # Full 8-operation crash/I/O matrix is a separate C harness, not claimed here.
(r/'build/ntfs3g-independent-details.log').write_text('\n'.join(logs))
print(f'PASS NTFS upstream integration: {checks} assertions/commands; independent ntfscat, ntfsls, ntfsinfo and ntfsfix -n; system/hibernation/nonempty guards. NOT Windows chkdsk or full NTFS certification.')
