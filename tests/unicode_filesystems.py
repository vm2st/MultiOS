#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — Unicode RW interoperability, malformed metadata and actual volume upcase policy.
from pathlib import Path
import subprocess,os,shutil,struct,hashlib,sys
os.environ['LC_ALL']='C.utf8'
ROOT=Path(__file__).resolve().parents[1];W=ROOT/'build/unicode-tests';W.mkdir(exist_ok=True)
HOST=Path(os.environ.get('MULTIOS_HOST',ROOT/'build/multios-host'));logs=[];checks=0

def ck(v,msg):
 global checks
 checks+=1
 if not v:raise AssertionError(msg)
def cmd(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=90)
 ck(p.returncode==0,(args,p.stdout,p.stderr));return p.stdout.decode('utf-8','replace')+p.stderr.decode('utf-8','replace')
def run(img,commands,errors=0,write=True):
 p=subprocess.run([str(HOST),str(img)]+(['--allow-write'] if write else []),input=('\n'.join(commands+['exit'])+'\n').encode(),capture_output=True,timeout=120)
 text=p.stdout.decode('utf-8','replace');logs.append(text+p.stderr.decode('utf-8','replace'))
 ck(p.returncode==0 and b'AddressSanitizer' not in p.stderr and b'runtime error:' not in p.stderr,(p.returncode,text,p.stderr))
 ck(text.count('Error:')==errors,text);return text
def digest(p):return hashlib.sha256(p.read_bytes()).digest()
def u16(b,o):return struct.unpack_from('<H',b,o)[0]
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
def p16(b,o,v):struct.pack_into('<H',b,o,v)
def p32(b,o,v):struct.pack_into('<I',b,o,v)
def checksum(b,skip=()):
 x=0
 for i,c in enumerate(b):
  if i not in skip:x=(((x>>1)|(x<<31))+c)&0xffffffff
 return x
# Root exFAT fixture uses one cluster and a contiguous recommended upcase table.
def exmeta(b):
 base=2048*512;spc=1<<b[base+109];heap=base+u32(b,base+88)*512;root=heap+(u32(b,base+96)-2)*spc*512
 up=next(p for p in range(root,root+spc*512,32) if b[p]==0x82)
 upstart=heap+(u32(b,up+20)-2)*spc*512;size=u32(b,up+24)
 return root,spc*512,up,upstart,size

def main():
 fat=W/'fat.img'
 with fat.open('wb') as f:f.truncate(64*1024*1024)
 cmd('mkfs.fat','-F','32','-s','1',fat)
 exfat=W/'exfat.img';shutil.copyfile(ROOT/'build/multios.img',exfat)
 payload=W/'oracle.txt';payload.write_text('EXTERNAL-UTF8\n')
 cmd('mcopy','-i',fat,payload,'::Внешний_αβγ.txt')
 s=run(fat,['type ВНЕШНИЙ_ΑΒΓ.TXT'],write=False);ck('EXTERNAL-UTF8' in s,'FAT external UTF-16 LFN read/casefold')
 long='😀'*60+'.txt';assert len(long.encode())==244
 mixed='é漢字_😀_αβγ.txt'
 for kind,img in [('fat32',fat),('exfat',exfat)]:
  commands=['unlock YES','md Документы','write Документы/Привет😀.txt "UNICODE-PAYLOAD"','type ДОКУМЕНТЫ/ПРИВЕТ😀.TXT',
   'append Документы/привет😀.txt "SECOND"','copy Документы/Привет😀.txt Документы/Копия.txt',
   'ren Документы/Копия.txt "Новое имя😀.txt"','cmp Документы/Привет😀.txt "Документы/Новое имя😀.txt"',
   'write '+mixed+' "MIXED"','write '+long+' "LONG"','type '+long,'touch café.txt','touch café.txt','lock','unlock YES',
   'tree','stat64 Документы/ПРИВЕТ😀.TXT','readat Документы/Привет😀.txt 0 15']
  s=run(img,commands);ck('UNICODE-PAYLOAD' in s and 'Identical' in s and 'MIXED' in s and 'LONG' in s,kind+' RW/case/nested')
  # A case-only spelling must not create a second file or change data.
  before=digest(img);s=run(img,['unlock YES','touch ДОКУМЕНТЫ/ПРИВЕТ😀.TXT'],1);ck('already exists' in s,kind+' case collision')
  # A clean unlock/lock can update metadata flags, but no second entry is allowed.
  s=run(img,['dir Документы']);ck(s.count('      Привет😀.txt')==1,kind+' single casefolded name')
  if kind=='fat32':
   # mtools 4.x does not resolve surrogate-pair LFNs; verify through ASCII SFNs
   # AND an independent UTF-16/FAT-chain parser, never treat its fallback as OS data.
   b=img.read_bytes();reserved=u16(b,14);fsz=u32(b,36);heap=(reserved+b[16]*fsz)*512;cluster_bytes=b[13]*512
   def chain(first):
    seen=set();data=b''
    while first<0xffffff8:
     ck(first>=2 and first not in seen,'FAT oracle cycle/bounds');seen.add(first)
     data+=b[heap+(first-2)*cluster_bytes:heap+(first-1)*cluster_bytes];first=u32(b,reserved*512+first*4)&0xfffffff
    return data
   def entries(first):
    raw=chain(first);parts=[];result={}
    for pos in range(0,len(raw),32):
     e=raw[pos:pos+32]
     if not e[0]:break
     if e[0]==229:parts=[];continue
     if e[11]==15:
      parts.insert(0,b''.join(e[j:j+2] for j in (1,3,5,7,9,14,16,18,20,22,24,28,30)));continue
     alias=e[:8].decode('ascii').rstrip();ext=e[8:11].decode('ascii').rstrip();alias+=('.'+ext) if ext else ''
     name=b''.join(parts).decode('utf-16le').rstrip('\0\uffff') if parts else alias;parts=[]
     result[name]=(alias,(u16(e,20)<<16)|u16(e,26),u32(e,28))
    return result
   root_entries=entries(u32(b,44));da,dc,ds=root_entries['Документы'];file_entries=entries(dc);fa,fc,fs=file_entries['Привет😀.txt']
   ck(chain(fc)[:fs]==b'UNICODE-PAYLOAD\nSECOND\n','independent FAT Unicode UTF-16 + content')
   la,lc,ls=root_entries[long];ck(chain(lc)[:ls]==b'LONG\n','independent FAT non-BMP long name + data')
   ck('UNICODE-PAYLOAD\nSECOND\n'==cmd('mtype','-i',img,'::'+da+'/'+fa),'mtools content via SFN')
   ck('LONG\n'==cmd('mtype','-i',img,'::'+la),'mtools non-BMP file via SFN')
   logs.append(cmd('fsck.fat','-n',img))
  else:
   # Independent byte-level oracle for exact UTF-16 unit lengths and exFAT name hashes.
   b=img.read_bytes();root,size,up,upstart,upsize=exmeta(b);table=list(range(65536));words=struct.unpack('<%dH'%(upsize//2),b[upstart:upstart+upsize]);i=c=0
   while c<65536:
    v=words[i];i+=1
    if v==65535 and c!=65535:c+=words[i];i+=1
    else:table[c]=v;c+=1
   found=set();pos=root
   while pos<root+size and b[pos]:
    if b[pos]!=0x85:pos+=32;continue
    n=b[pos+1]+1;item=b[pos:pos+32*n];units=item[35];raw=b''.join(item[j*32+2:j*32+32] for j in range(2,n))[:units*2];name=raw.decode('utf-16le');found.add(name)
    h=0
    for (v,) in struct.iter_unpack('<H',raw):
     for octet in struct.pack('<H',table[v]):h=(((h>>1)|(h<<15))+octet)&65535
    ck(h==u16(item,36),'independent Unicode NameHash '+name);ck(n==2+(units+14)//15,'UTF-16 secondary count')
    pos+=32*n
   ck({mixed,long,'café.txt','café.txt','Документы'}<=found,'exFAT exact UTF-16 names')
   logs.append(cmd('fsck.exfat','-n',str(img)+'-volume') if False else '')
   part=W/'checked.exfat';part.write_bytes(b[2048*512:]);logs.append(cmd('fsck.exfat','-n',part))
  s=run(img,['unlock YES','ren "Документы/Новое имя😀.txt" итог.txt','del Документы/итог.txt','del Документы/Привет😀.txt','rd Документы','del '+mixed,'del '+long,'del café.txt','del café.txt','lock','unlock YES'])
  if kind=='fat32':logs.append(cmd('fsck.fat','-n',img))
  else:part=W/'checked.exfat';part.write_bytes(img.read_bytes()[2048*512:]);logs.append(cmd('fsck.exfat','-n',part))
 # Use the NTFS fixture created independently by ntfsprogs/libntfs-3g.
 sys.path.insert(0,str(ROOT/'tests'));from ntfs_fixture import populate
 ntfs=W/'ntfs.img'
 with ntfs.open('wb') as f:f.truncate(64*1024*1024)
 cmd('mkntfs','-F','-Q','-s','512',ntfs);populate(ntfs);before=digest(ntfs)
 s=run(ntfs,['dir','type unicode_привет.TXT','stat64 Unicode_ПРИВЕТ.txt','unlock YES'],1)
 ck('Unicode_Привет.txt' in s and 'Unicode metadata' in s,'NTFS Unicode names + real $UpCase');ck(before==digest(ntfs),'NTFS Unicode never writes')
 custom_ntfs=W/'ntfs-custom.img';shutil.copyfile(ntfs,custom_ntfs)
 table=subprocess.run(['ntfscat',str(custom_ntfs),'/$UpCase'],capture_output=True,timeout=60)
 ck(table.returncode==0 and len(table.stdout)==131072,'NTFS external $UpCase oracle')
 table_bytes=bytearray(table.stdout);p16(table_bytes,0x440*2,0x41f);tablefile=W/'ntfs-upcase.bin';tablefile.write_bytes(table_bytes)
 cmd('ntfscp','-f',custom_ntfs,tablefile,'/$UpCase');before=digest(custom_ntfs)
 s=run(custom_ntfs,['type Unicode_рривет.txt']);ck('Unicode metadata' in s,'actual NTFS custom $UpCase, not a built-in approximation');ck(digest(custom_ntfs)==before,'custom NTFS source unchanged')
 # A wrong mandatory ASCII mapping must fail mount even if the stream has the expected size.
 p16(table_bytes,ord('a')*2,ord('X'));tablefile.write_bytes(table_bytes);cmd('ntfscp','-f',custom_ntfs,tablefile,'/$UpCase');before=digest(custom_ntfs)
 s=run(custom_ntfs,['unlock YES','touch denied'],2);ck(before==digest(custom_ntfs),'invalid NTFS case table never written')
 # Actual exFAT table: change lowercase Cyrillic я -> uppercase А, NOT recommended Я.
 custom=W/'custom-upcase.img';shutil.copyfile(ROOT/'build/multios.img',custom);b=bytearray(custom.read_bytes());root,size,up,start,length=exmeta(b)
 c=off=0;word=None
 while c<65536:
  v=u16(b,start+off);off+=2
  if v==65535 and c!=65535:c+=u16(b,start+off);off+=2
  else:
   if c==0x44f:word=start+off-2
   c+=1
 ck(word is not None,'custom table explicit entry');p16(b,word,0x410);p32(b,up+4,checksum(b[start:start+length]));custom.write_bytes(b)
 s=run(custom,['unlock YES','write А.txt "CUSTOM-TABLE"','type я.txt','touch я.txt'],1)
 ck('CUSTOM-TABLE' in s and 'already exists' in s,'exFAT uses actual compressed on-volume mapping')
 # Compressed run cannot be zero; checksum is deliberately recomputed, so parser must reject.
 broken=bytearray(b);off=next(i for i in range(0,length-2,2) if u16(b,start+i)==65535);p16(broken,start+off+2,0);p32(broken,up+4,checksum(broken[start:start+length]));bad=W/'bad-upcase.img';bad.write_bytes(broken);before=digest(bad)
 s=run(bad,['unlock YES','touch denied'],2);ck(before==digest(bad),'malformed compressed table changed no bytes')
 # A malformed UTF-8 command must never be executed after silently dropping bytes.
 before=digest(fat);p=subprocess.run([str(HOST),str(fat),'--allow-write'],input=b'unlock YES\ntouch danger\xc0\xaf.txt\nexit\n',capture_output=True,timeout=60)
 ck(p.returncode==0 and b'not executed' in p.stdout,'malformed UTF-8 command rejected');s=run(fat,['type danger.txt'],1)
 tag='san-' if os.environ.get('MULTIOS_HOST') else ''
 report=f'PASS {checks} Unicode filesystem/interoperability/safety assertions; FAT32 + exFAT create/read/append/copy/rename/delete/directories; Cyrillic, Greek, CJK, non-BMP; NTFS read-only Unicode; actual custom exFAT + NTFS upcase; malformed-table/input refusal. No full NTFS RW or power-loss certification.\n'
 (ROOT/'build'/('unicode-'+tag+'filesystem-report.txt')).write_text(report);(ROOT/'build'/('unicode-'+tag+'filesystem-console.log')).write_text('\n'.join(logs));print(report)
if __name__=='__main__':main()
