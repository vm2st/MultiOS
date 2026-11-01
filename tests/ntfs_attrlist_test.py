#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — independent NTFS extension records + hostile lists.
from pathlib import Path
import os,subprocess,struct,hashlib,zlib,re,sys
from ntfs_fixture import populate
from ntfs_attrlist_fixture import populate_lists
R=Path(__file__).resolve().parents[1];W=R/'build/ntfs2-tests';W.mkdir(exist_ok=True)
logs=[];checks=0

def check(v,msg):
 global checks
 checks+=1
 if not v:raise AssertionError(msg)
def cmd(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=90);check(p.returncode==0,str(args)+'\n'+p.stderr.decode(errors='replace'));return p.stdout

def host(image,commands,errors=0,sanitize=False):
 exe=R/'build'/('multios-sanitize-host' if sanitize else 'multios-host')
 p=subprocess.run([str(exe),str(image),'--allow-write'],input='\n'.join(commands+['exit'])+'\n',capture_output=True,text=True,timeout=90)
 logs.append(p.stdout+p.stderr);check(p.returncode==0,p.stdout+p.stderr);check('AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr,p.stderr)
 check(p.stdout.count('Error:')==errors,p.stdout);return p.stdout

def attrs(b):
 p=struct.unpack_from('<H',b,20)[0]
 while struct.unpack_from('<I',b,p)[0]!=0xffffffff:
  n=struct.unpack_from('<I',b,p+4)[0];assert n>=24 and p+n<=len(b)
  yield p;p+=n

def decode(b):
 b=bytearray(b);o,n=struct.unpack_from('<HH',b,4);assert n==len(b)//512+1
 for i in range(1,n):assert b[i*512-2:i*512]==b[o:o+2];b[i*512-2:i*512]=b[o+2*i:o+2*i+2]
 return b

def encode(b):
 b=bytearray(b);o,n=struct.unpack_from('<HH',b,4)
 for i in range(1,n):b[o+2*i:o+2*i+2]=b[i*512-2:i*512];b[i*512-2:i*512]=b[o:o+2]
 return b

def runs(b,p):
 q=p+struct.unpack_from('<H',b,p+32)[0];v=struct.unpack_from('<Q',b,p+16)[0];l=0
 while b[q]:
  h=b[q];q+=1;nc=h&15;nl=h>>4;n=int.from_bytes(b[q:q+nc],'little');q+=nc
  if nl:l+=int.from_bytes(b[q:q+nl],'little',signed=True);q+=nl
  yield v,l,n;v+=n

def main():
 image=W/'attributes.ntfs'
 with image.open('wb') as f:f.truncate(64*1024*1024)
 cmd('mkntfs','-F','-Q','-s','512',image);populate(image);populate_lists(image,160)
 cmd(R/'build/ntfs-split-fixture',image)
 proof=''
 for n in ('resident.txt','fragmented.bin','split.bin'):
  t=cmd('ntfsinfo','-f','-v','-F','/'+n,image).decode();(W/(n+'.info')).write_text(t);proof+=t
 check('Lowest VCN\t\t\t 80' in proof or re.search(r'Lowest VCN\s+80',proof),'No real second DATA extent')
 check(len(set(re.findall(r'Dumping attribute \$DATA.*record (\d+)',(W/'split.bin.info').read_text())))==2,'Independent extension DATA proof')
 raw=image.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 names=('resident.txt','hardlink.txt','fragmented.bin','sparse.bin','uninitialized.bin','split.bin')
 commands=['fs','dir','type resident.txt','type DOCS/Nested.txt','cmp resident.txt hardlink.txt']
 for n in names:commands+=['crc32 '+n]
 commands+=['unlock YES','touch denied','write resident.txt "forbidden"','del resident.txt','md denied','ren split.bin other.bin']
 for san in (False,True):
  text=host(image,commands,6,san)
  for n in names:
   external=cmd('ntfscat',image,'/'+n);check(f'{zlib.crc32(external):08X}' in text,'External CRC '+n)
  check(hashlib.sha256(image.read_bytes()).hexdigest()==digest,'Read/refused mutation altered volume')
 cmd('ntfsfix','-n',image)
 # Independent, small test-only MFT walker. Does not reuse the OS parser/codecs.
 cluster=raw[13]*512;code=struct.unpack_from('<b',raw,64)[0];size=(1<<-code) if code<0 else code*cluster
 mft0=struct.unpack_from('<Q',raw,48)[0]*cluster;m0=decode(raw[mft0:mft0+size]);dp=next(p for p in attrs(m0) if struct.unpack_from('<I',m0,p)[0]==0x80 and not m0[p+9]);mmap=list(runs(m0,dp))
 def location(number):
  off=number*size;v=off//cluster
  for a,l,n in mmap:
   if a<=v<a+n:return (l+v-a)*cluster+off%cluster
  raise AssertionError('MFT location missing')
 def record(number):p=location(number);return decode(raw[p:p+size])
 def number(n):return int(re.search(r'Dumping Inode (\d+)',cmd('ntfsinfo','-f','-F','/'+n,image).decode()).group(1))
 base=number('split.bin');br=record(base);lp=next(p for p in attrs(br) if struct.unpack_from('<I',br,p)[0]==0x20);check(not br[lp+8],'split fixture list should be resident')
 start=lp+struct.unpack_from('<H',br,lp+20)[0];length=struct.unpack_from('<I',br,lp+16)[0];entries=[];p=start
 while p<start+length:
  if struct.unpack_from('<I',br,p)[0]==0x80 and br[p+6]==0:entries.append(p)
  p+=struct.unpack_from('<H',br,p+4)[0]
 check(len(entries)==2,'Need two unnamed DATA list entries');e0,e1=entries;ext=struct.unpack_from('<Q',br,e1+16)[0]&0xffffffffffff;er=record(ext);ep=next(p for p in attrs(er) if struct.unpack_from('<I',er,p)[0]==0x80)
 def p16(b,o,v):struct.pack_into('<H',b,o,v)
 def p64(b,o,v):struct.pack_into('<Q',b,o,v)
 cases=[]
 def badbase(label,mut):b=bytearray(br);mut(b);cases.append((label,location(base),encode(b)))
 def badext(label,mut):b=bytearray(er);mut(b);cases.append((label,location(ext),encode(b)))
 badbase('stale-reference-sequence',lambda b:p16(b,e1+22,0x7fff))
 badbase('wrong-instance-id',lambda b:p16(b,e1+24,0x7fff))
 badbase('48-bit-record-outside-MFT',lambda b:p64(b,e1+16,0x0001ffffffffffff))
 badbase('VCN-gap',lambda b:p64(b,e1+8,81))
 badbase('VCN-overlap',lambda b:p64(b,e1+8,79))
 badbase('VCN-duplicate',lambda b:p64(b,e1+8,0))
 badbase('list-entry-short',lambda b:p16(b,e1+4,24))
 badbase('list-entry-overflow',lambda b:p16(b,e1+4,0xfff8))
 badbase('list-name-overlap',lambda b:(b.__setitem__(e1+6,1),b.__setitem__(e1+7,24)))
 badbase('missing-first-DATA',lambda b:struct.pack_into('<I',b,e0,0x90))
 badbase('missing-final-DATA',lambda b:struct.pack_into('<I',b,e1,0x90))
 badext('wrong-base-record',lambda b:p64(b,32,base+1))
 badext('wrong-base-sequence',lambda b:p64(b,32,base|(0x7fff<<48)))
 badext('inactive-extension',lambda b:p16(b,22,0))
 badext('directory-extension',lambda b:p16(b,22,3))
 badext('wrong-extension-sequence',lambda b:p16(b,16,0x7ffe))
 badext('extent-low-VCN-mismatch',lambda b:p64(b,ep+16,81))
 badext('extent-high-VCN-mismatch',lambda b:p64(b,ep+24,160))
 badext('mapping-outside-attribute',lambda b:p16(b,ep+32,0xffff))
 badext('mapping-run-outside-volume',lambda b:b.__setitem__(ep+struct.unpack_from('<H',b,ep+32)[0],0x88))
 badext('compression-refused',lambda b:p16(b,ep+12,1))
 badext('nested-list-refused',lambda b:struct.pack_into('<I',b,ep,0x20))
 broken=bytearray(raw[location(ext):location(ext)+size]);broken[510]^=1;cases.append(('USA-mismatch',location(ext),broken))
 # Nonresident ATTRIBUTE_LIST bounds and physical mapping corruption.
 fn=number('fragmented.bin');fr=record(fn);fp=next(p for p in attrs(fr) if struct.unpack_from('<I',fr,p)[0]==0x20);check(fr[fp+8]==1,'Expected nonresident list')
 for label,change in [('list-over-16KiB',lambda b:p64(b,fp+48,16385)),('list-uninitialized',lambda b:p64(b,fp+56,0)),('list-high-VCN-mismatch',lambda b:p64(b,fp+24,0x100000000))]:
  b=bytearray(fr);change(b);cases.append((label,location(fn),encode(b)))
 # For each corrupt source both ordinary + sanitized runtime must reject safely.
 for label,off,replacement in cases:
  corrupted=bytearray(raw);corrupted[off:off+len(replacement)]=replacement;p=W/'bad-attributes.ntfs';p.write_bytes(corrupted);before=hashlib.sha256(corrupted).digest()
  target='fragmented.bin' if label.startswith('list-') and label in ('list-over-16KiB','list-uninitialized','list-high-VCN-mismatch') else 'split.bin'
  for san in (False,True):host(p,['crc32 '+target],1,san)
  check(hashlib.sha256(p.read_bytes()).digest()==before,label+' modified source')
  logs.append('PASS corruption: '+label)
 (R/'build/phase2-attrlist-report.txt').write_text(f'PASS {checks} checks, {len(cases)} independently mutated volumes in normal + ASan/UBSan hosts.\nIndependent libntfs-3g fixtures: resident DATA in extension; nonresident 7808-byte attribute list; two DATA extents (VCN 0..79 and 80..159).\nCRC32 of six streams equals ntfscat. NTFS write attempts refused; all tested source bytes unchanged.\nNo NTFS write support or hardware certification claimed.\n')
 (R/'build/phase2-attrlist-console.log').write_text('\n'.join(logs));print((R/'build/phase2-attrlist-report.txt').read_text())
if __name__=='__main__':main()
