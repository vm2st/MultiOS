#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — black-box external-format FAT32/NTFS tests.
from pathlib import Path
import os,sys,subprocess,shutil,hashlib,struct,zlib,tempfile
from ntfs_fixture import populate
ROOT=Path(__file__).resolve().parents[1];host=os.environ.get('MULTIOS_HOST',str(ROOT/'build/multios-host'))
work=ROOT/'build/fs11-tests';work.mkdir(exist_ok=True);logs=[];checks=0

def check(x,msg):
 global checks
 checks+=1
 if not x:raise AssertionError(msg)
def cmd(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,text=True,timeout=60)
 if p.returncode:raise AssertionError(str(args)+'\n'+p.stdout+p.stderr)
 return p.stdout+p.stderr
def run(image,commands,errors=0,write=True):
 p=subprocess.run([host,str(image)]+(['--allow-write'] if write else []),input='\n'.join(commands+['exit'])+'\n',capture_output=True,text=True,timeout=90)
 logs.append(p.stdout+p.stderr);check(p.returncode==0,p.stdout+p.stderr)
 check(p.stdout.count('Error:')==errors,p.stdout+p.stderr)
 check('AddressSanitizer' not in p.stderr and 'runtime error:' not in p.stderr,p.stderr)
 return p.stdout
def digest(p):return hashlib.sha256(p.read_bytes()).digest()
def fresh(name,kind):
 p=work/name
 with p.open('wb') as f:f.truncate(64*1024*1024)
 cmd('mkfs.fat','-F','32','-s','1',p) if kind=='fat' else cmd('mkntfs','-F','-Q','-s','512',p)
 return p
def copy(p,name):q=work/name;shutil.copyfile(p,q);return q

def main():
 fat=fresh('fat.img','fat');source=work/'external.txt';source.write_text('External FAT32\n')
 cmd('mcopy','-i',fat,source,'::External long filename.txt')
 big=work/'big.bin';payload=bytes(range(256))*600;big.write_bytes(payload);cmd('mcopy','-i',fat,big,'::big.bin')
 before=digest(fat)
 s=run(fat,['fs','dir','type "External long filename.txt"','unlock YES','touch denied'],2,False)
 check('FAT32 mounted' in s and 'External FAT32' in s,'FAT RO/read');check(digest(fat)==before,'RO changed FAT')
 commands=['unlock YES','md DOCS','write DOCS/a.txt "First"','append DOCS/a.txt "Second"','copy DOCS/a.txt DOCS/copy.txt','ren DOCS/copy.txt "long renamed file.txt"','cmp DOCS/a.txt "DOCS/long renamed file.txt"','copy big.bin DOCS/bigcopy.bin','crc32 DOCS/bigcopy.bin']
 commands += [f'touch "DOCS/Long generated filename {j}.txt"' for j in range(40)]
 commands += ['touch "'+('a'*200)+'.txt"','lock','unlock YES','type DOCS/a.txt','df','fs']
 s=run(fat,commands);check('First\nSecond' in s and 'Identical' in s,'FAT write/copy/append');check(f'{zlib.crc32(payload):08X}' in s,'FAT multi-cluster CRC')
 check('FAT32' in s and 'Mode: RW' in s,'FAT audit after directory growth')
 check('First\nSecond\n'==cmd('mtype','-i',fat,'::DOCS/a.txt'),'independent FAT contents')
 logs.append(cmd('fsck.fat','-n',fat))
 s=run(fat,['unlock YES','touch DOCS/a.txt','rd DOCS','del DOCS','write "bad?.txt" "x"','ren DOCS/a.txt bigcopy.bin','cd DOCS/a.txt'],6)
 cmd('mattrib','-i',fat,'+r','::DOCS/a.txt');s=run(fat,['unlock YES','write DOCS/a.txt "forbidden"','del DOCS/a.txt','ren DOCS/a.txt other.txt'],3)
 cmd('mattrib','-i',fat,'-r','::DOCS/a.txt')
 commands=['unlock YES','del DOCS/a.txt','del "DOCS/long renamed file.txt"','del DOCS/bigcopy.bin']+[f'del "DOCS/Long generated filename {j}.txt"' for j in range(40)]+['rd DOCS','lock','unlock YES']
 run(fat,commands);logs.append(cmd('fsck.fat','-n',fat))
 unicode=copy(fat,'unicode-fat.img');cmd('mcopy','-i',unicode,source,'::Unicode_Привет.txt');before=digest(unicode)
 s=run(unicode,['dir','type Unicode_привет.txt']);check('Unicode_Привет.txt' in s and 'External FAT32' in s,'Unicode lookup/listing');check(digest(unicode)==before,'Unicode RO changed disk')
 # Valid short aliases remain usable even when the corresponding LFN is Unicode.
 b=unicode.read_bytes();reserved=struct.unpack_from('<H',b,14)[0];fsz=struct.unpack_from('<I',b,36)[0];heap=(reserved+b[16]*fsz)*512;alias=None
 cluster=struct.unpack_from('<I',b,44)[0]
 for step in range(64):
  start=heap+(cluster-2)*b[13]*512
  for pos in range(start,start+b[13]*512,32):
   e=b[pos:pos+32]
   if e[0] not in (0,0xe5) and e[11]!=0x0f and e[11]&8==0 and b'UNICOD' in e[:8]:alias=e[:8].decode().rstrip()+'.'+e[8:11].decode().rstrip()
  nextcl=struct.unpack_from('<I',b,reserved*512+cluster*4)[0]&0x0fffffff
  if nextcl>=0x0ffffff8:break
  cluster=nextcl
 check(alias is not None,'Unicode alias fixture missing')
 s=run(unicode,[f'type {alias}']);check('External FAT32' in s,'Unicode short alias lookup')
 # Directory depth limit: valid 16, reject 17, then re-audit successfully.
 depth=copy(fat,'depth-fat.img');commands=['unlock YES'];path=''
 for j in range(16):path+='/d';commands.append('md '+path)
 commands+=['md '+path+'/d','lock','unlock YES','write '+path+'/deep.txt "depth16"','type '+path+'/deep.txt']
 s=run(depth,commands,1);check('depth16' in s,'FAT depth limit/re-audit');logs.append(cmd('fsck.fat','-n',depth))
 # FAT hostile geometry, mirrors, chains and directory ownership: must never alter bytes.
 clean=fresh('clean-fat.img','fat');cmd('mcopy','-i',clean,source,'::file.txt');base=clean.read_bytes();r=struct.unpack_from('<H',base,14)[0];f=struct.unpack_from('<I',base,36)[0];h=(r+base[16]*f)*512
 filepos=next(p for p in range(h,h+512,32) if base[p] not in (0,0xe5) and base[p+11] not in (8,15))
 fc=struct.unpack_from('<H',base,filepos+26)[0]
 def p32(b,off,v):struct.pack_into('<I',b,off,v)
 def fatval(b,c,value):
  for k in range(base[16]):p32(b,(r+k*f)*512+c*4,value)
 mutations={
 'sector-size':lambda b:struct.pack_into('<H',b,11,4096),
 'spc-zero':lambda b:b.__setitem__(13,0),
 'short-total':lambda b:p32(b,32,16),
 'root-outside':lambda b:p32(b,44,0x0ffffff0),
 'mirror-mismatch':lambda b:p32(b,(r+f)*512+fc*4,0),
 'backup-mismatch':lambda b:b.__setitem__(6*512+3,ord('!')),
 'bad-fsinfo':lambda b:p32(b,512,0),
 'bad-backup-fsinfo':lambda b:p32(b,7*512,0),
 'root-cycle':lambda b:fatval(b,2,2),
 'free-file':lambda b:fatval(b,fc,0),
 'file-cycle':lambda b:fatval(b,fc,fc),
 'file-root-crosslink':lambda b:struct.pack_into('<H',b,filepos+26,2),
 'dirty':lambda b:fatval(b,1,0x07ffffff),
 'bad-data-cluster':lambda b:struct.pack_into('<H',b,filepos+26,0xffff),
 }
 for label,mutate in mutations.items():
  b=bytearray(base);mutate(b);p=work/'bad-fat.img';p.write_bytes(b);before=digest(p)
  s=run(p,['unlock YES','touch denied.txt'],2)
  check('EXPERIMENTAL WRITE ENABLED' not in s,label);check(digest(p)==before,label+' changed bytes')
 # NTFS fixtures are formatted/populated by independent ntfsprogs/libntfs-3g.
 ntfs=fresh('ntfs.img','ntfs');populate(ntfs)
 cmd('ntfscp','-f','-N','Secret',ntfs,source,'/resident.txt')
 before=digest(ntfs)
 s=run(ntfs,['fs','dir','tree','type DOCS/Nested.txt','type hardlink.txt','cmp hardlink.txt resident.txt','crc32 fragmented.bin','crc32 sparse.bin','df','stat resident.txt','unlock YES','touch denied','md denied','del resident.txt','ren resident.txt other.txt','write resident.txt "bad"','type resident.txt:Secret','cd linkdir'],8)
 check('NTFS mounted' in s and 'Nested NTFS directory' in s and 'Resident NTFS' in s and 'Identical' in s,'NTFS content/links/dirs')
 for n in ('resident.txt','fragmented.bin','sparse.bin','uninitialized.bin'):
  p=subprocess.run(['ntfscat',str(ntfs),'/'+n],capture_output=True,timeout=60);check(p.returncode==0,p.stderr)
  check(f'{zlib.crc32(p.stdout):08X}' in run(ntfs,['crc32 '+n]),'NTFS independent CRC '+n)
 check('MFT record:' in s and 'READ-ONLY backend' in s,'NTFS diagnostics');check(digest(ntfs)==before,'NTFS source was modified')
 logs.append(cmd('ntfsfix','-n',ntfs))
 # Boot/MFT mutations: either deny mount or later access, never write/crash.
 nbase=ntfs.read_bytes();mft=struct.unpack_from('<Q',nbase,48)[0]*nbase[13]*512
 cases={'bad-bps':(11,b'\x00\x10'),'bad-spc':(13,b'\x00'),'bad-total':(40,struct.pack('<I',0xffffffff)),'bad-mft-lcn':(48,struct.pack('<I',0xffffffff)),'bad-record-code':(64,b'\xf0'),'bad-USA-trailer':(mft+510,b'\x00\x00'),'bad-attr-offset':(mft+20,b'\xff\xff')}
 for label,(off,data) in cases.items():
  b=bytearray(nbase);b[off:off+len(data)]=data;p=work/'bad-ntfs.img';p.write_bytes(b);before=digest(p)
  s=run(p,['unlock YES','touch denied'],2);check(digest(p)==before,label+' modified source')
 # Explicit partition selection never treats the ESP as the default data volume.
 unified=ROOT/'build/MultiOS-MultiPulti.img'
 if unified.exists():
  p=copy(unified,'mounts.img');before=digest(p)
  s=run(p,['fs','mount 1','fs','dir','mount 2','fs','mount 4','fs','mount auto','fs'],1)
  with unified.open('rb') as f:f.seek(133120*512);boot=f.read(512)
  data_kind='FAT32' if boot[82:90]==b'FAT32   ' else 'NTFS' if boot[3:11]==b'NTFS    ' else 'exFAT'
  check('FAT32 mounted read-only' in s and data_kind+' mounted read-only' in s and 'No filesystem mounted' in s,'mount switching/failure')
  check(digest(p)==before,'mount commands modified image')
 report=f'{checks} FAT32/NTFS assertions PASS. External formatting/content checks; FAT32 RW/growth/depth/RO/14 corruptions; NTFS directories, hardlinks, resident/nonresident/sparse/uninitialized/ADS/reparse isolation, write prohibition/7 corruptions; VFS MBR selection. Emulated/host tests, not full filesystem or power-loss certification.\n'
 tag='sanitize-' if os.environ.get('MULTIOS_HOST') else ''
 (ROOT/'build'/('dev11-filesystems-'+tag+'report.txt')).write_text(report)
 (ROOT/'build'/('dev11-filesystems-'+tag+'logs.txt')).write_text('\n'.join(logs))
 print(report)
if __name__=='__main__':main()
