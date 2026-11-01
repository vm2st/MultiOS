#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — USA encode/decode against independent NTFS volumes.
# Only ordinary throwaway TEST files are modified by this host-side harness.
from pathlib import Path
import ctypes as c,hashlib,shutil,struct,subprocess,sys
from ntfs_fixture import populate
R=Path(__file__).resolve().parents[1];W=R/'build/ntfs1-volume';W.mkdir(exist_ok=True);logs=[]
def cmd(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=120);logs.append(p.stdout+p.stderr)
 assert not p.returncode,(args,p.stdout,p.stderr)
 return p.stdout
source=W/'original.ntfs'
with source.open('wb') as f:f.truncate(64*1024*1024)
cmd('mkntfs','-F','-Q','-s','512',source);populate(source)
before=hashlib.sha256(source.read_bytes()).hexdigest()
shared=W/'metadata.so';cmd('gcc','-shared','-fPIC','-std=c11','-I'+str(R/'lib'),str(R/'lib/ntfs_meta.c'),'-o',shared)
lib=c.CDLL(str(shared));array=c.c_ubyte*4096
for name in ('ntfs_usa_decode','ntfs_usa_encode'):
 f=getattr(lib,name);f.restype=c.c_int;f.argtypes=[c.c_void_p,c.c_uint32,c.c_uint32]
raw=source.read_bytes();bps=struct.unpack_from('<H',raw,11)[0];spc=raw[13];code=struct.unpack_from('<b',raw,64)[0];record=(1<<-code) if code<0 else code*bps*spc
assert bps==512 and record<=4096
clone=W/'usa-roundtrip.ntfs';shutil.copyfile(source,clone)
for label,field in [('MFT',48),('MFTMirr',56)]:
 off=struct.unpack_from('<Q',raw,field)[0]*bps*spc;entry=raw[off:off+record];assert entry[:4]==b'FILE'
 buf=array();buf[:record]=entry
 assert not lib.ntfs_usa_decode(buf,record,bps);decoded=bytes(buf[:record]);usa=struct.unpack_from('<H',entry,4)[0];num=struct.unpack_from('<H',entry,6)[0]
 # Independent check of restored end-of-sector bytes using raw USA array.
 for i in range(1,num):assert decoded[i*bps-2:i*bps]==entry[usa+2*i:usa+2*i+2]
 assert not lib.ntfs_usa_encode(buf,record,bps);encoded=bytes(buf[:record])
 seq=encoded[usa:usa+2];assert seq!=entry[usa:usa+2]
 for i in range(1,num):assert encoded[i*bps-2:i*bps]==seq
 for i in range(record):
  if not (usa<=i<usa+num*2) and i%bps not in (bps-2,bps-1):assert encoded[i]==entry[i]
 with clone.open('r+b') as f:f.seek(off);f.write(encoded)
 logs.append('PASS '+label+' record0 independent USA restoration and protected encoding; all other bytes preserved')
cmd('ntfsfix','-n',clone)
for n in ('resident.txt','fragmented.bin','sparse.bin','uninitialized.bin'):
 a=subprocess.run(['ntfscat',str(source),'/'+n],capture_output=True,timeout=90);b=subprocess.run(['ntfscat',str(clone),'/'+n],capture_output=True,timeout=90)
 assert not a.returncode and not b.returncode and a.stdout==b.stdout,n
p=subprocess.run([str(R/'build/multios-host'),str(clone),'--allow-write'],input='dir\ntype resident.txt\ncrc32 fragmented.bin\nunlock YES\ntouch denied\nexit\n',capture_output=True,text=True,timeout=90)
logs.append(p.stdout+p.stderr);assert not p.returncode and 'Resident NTFS' in p.stdout and p.stdout.count('Error:')==2,p.stdout+p.stderr
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
(W/'console.log').write_text('\n'.join(logs));report='PASS independent mkntfs/libntfs-3g volume: native USA codec roundtrip for MFT0 + MFTMirr0; ntfsfix -n accepts result; four file streams byte-identical via ntfscat; MultiOS reads result and refuses NTFS writes. Source image unchanged. Host harness only: no live OS write or journal replay implemented.\n'
(R/'build/phase1-metadata-volume-report.txt').write_text(report);print(report)
