#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — copy-only extended BIOS RW bundle installer.
# Cannot be used to enable unsafe RW on an unprepared / unknown image.
from pathlib import Path
import sys,os,stat,struct,zlib,shutil,subprocess,tempfile,argparse,fcntl
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from mkunified import sources_zip
from customize import DEFAULT,PRESETS,settings_text,profiles,padded,slot
crc=lambda b:zlib.crc32(b)&0xffffffff

def install(source,output,bundle=None,efi=None):
 source=Path(source);output=Path(output)
 if output.exists() or output.is_symlink():raise ValueError('Output exists; only NEW copies accepted')
 fd=os.open(source,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
 with os.fdopen(fd,'rb') as f:
  fcntl.flock(f,fcntl.LOCK_SH|fcntl.LOCK_NB);fcntl.lockf(f,fcntl.LOCK_SH|fcntl.LOCK_NB)
  st=os.fstat(f.fileno());n=st.st_size
  if not stat.S_ISREG(st.st_mode) or st.st_nlink!=1 or n%512 or not 2048*512<n<=0x10000000*512:raise ValueError('Regular aligned single-link image required')
  m=bytearray(f.read(512))
  if m[510:]!=b'\x55\xaa' or m[432:440] not in (b'MP11\xf0\x00\x01\x00',b'MP22\x10\x00\x01\x00'):raise ValueError('Unknown BIOS boot format')
  parts=[struct.unpack_from('<B3sB3sII',m,446+i*16) for i in range(4)]
  if min(p[4] for p in parts if p[2])!=2048:raise ValueError('Expected 1MiB reserved BIOS area')
  f.seek(n-1024);foot=f.read(512);other=f.read(512)
  if foot!=other or foot[:8]!=b'MPSTORE1' or crc(foot[:508])!=struct.unpack_from('<I',foot,508)[0] or struct.unpack_from('<I',foot,76)[0]!=crc(m):raise ValueError('Invalid/unequal storage footer/MBR binding')
  start=struct.unpack_from('<Q',foot,24)[0];f.seek(start*512);h=f.read(512);h2=f.read(512)
  if h!=h2 or h[:8]!=b'MPUNDO1\0' or crc(h[:508])!=struct.unpack_from('<I',h,508)[0] or struct.unpack_from('<I',h,20)[0]!=0:raise ValueError('Journal not clean; recover before installing')
  b=Path(bundle) if bundle else r/'build/bios-rw';boot=(b/'mbr.bin').read_bytes();stage=(b/'loader.bin').read_bytes();kernel=(b/'kernel.bin').read_bytes()
  if len(boot)!=512 or len(stage)>15*512 or not 16<=len(kernel)<=(2048-18)*512:raise ValueError('BIOS bundle does not fit reserved area')
  m[:440]=boot[:440];header=bytearray(512);header[:8]=b'MPRW22\0\0';struct.pack_into('<II',header,8,len(kernel),crc(kernel));header[24:28]=b'MC25'
  bios,efi_profile=profiles(DEFAULT)
  foot=bytearray(foot);struct.pack_into('<I',foot,76,crc(m));struct.pack_into('<I',foot,508,crc(foot[:508]))
  made=False
  try:
   with output.open('xb') as out:
    made=True;f.seek(0);shutil.copyfileobj(f,out,1024**2);out.seek(0);out.write(m);out.write(stage+bytes(15*512-len(stage))+slot(bios));out.write(header);out.write(kernel);out.write(bytes((2048-18)*512-len(kernel)))
    out.seek(n-1024);out.write(foot);out.write(foot);out.flush();os.fsync(out.fileno())
   after=os.fstat(f.fileno())
   if (after.st_size,after.st_mtime_ns,after.st_ctime_ns)!=(st.st_size,st.st_mtime_ns,st.st_ctime_ns):raise ValueError('Source changed while copying')
   # ESP must contain the matching BIOS payload and corresponding sources.
   with tempfile.TemporaryDirectory(prefix='bios-rw-install-') as td:
    src=Path(td)/'PORTING.ZIP';sources_zip(src);drive=str(output)+'@@1048576'
    subprocess.run(['mmd','-i',drive,'::EASY'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    entries=[]
    for name,text,dest in [('SETTINGS.INI',settings_text(DEFAULT),'EASY/SETTINGS.INI'),('BIOS.CFG',bios,'MULTIOS/BIOS.CFG'),('BOOT.CFG',efi_profile,'MULTIOS/BOOT.CFG')]+[(k.upper()+'.INI',settings_text(v),'EASY/'+k.upper().replace('TWO-BUTTONS','TWOBTN')+'.INI') for k,v in PRESETS.items()]:
     file=Path(td)/name;file.write_bytes(padded(text));entries.append((file,dest))
    entries += [(r/'tools/customize.py','EASY/CUSTOM.PY')]
    entries += [(b/'kernel.bin' ,'MULTIOS/KERNEL32.BIN'),(b/'loader.bin','MULTIOS/BLOADER.BIN'),(b/'mbr.bin','MULTIOS/BOOT32.BIN'),(src,'SOURCES/PORTING.ZIP')]
    if efi:
     ef=Path(efi);raw=ef.read_bytes()
     if raw[:2]!=b'MZ' or len(raw)<256:raise ValueError('Expected x64 EFI PE image')
     pe=struct.unpack_from('<I',raw,60)[0]
     if raw[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',raw,pe+4)[0]!=0x8664:raise ValueError('Not x64 PE')
     entries.append((ef,'EFI/BOOT/BOOTX64.EFI'))
    doc=r/'FILESYSTEMS_0_37_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_36_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_35_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_34_RU.md'
    if not doc.exists():doc=r/'INPUT_0_33_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_32_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_31_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_30_RU.md'
    if not doc.exists():doc=r/'FILESYSTEMS_0_29_RU.md'
    if not doc.exists():doc=r/'USB_STORAGE_0_28_RU.md'
    if not doc.exists():doc=r/'USB_STORAGE_0_27_RU.md'
    if not doc.exists():doc=r/'EASY_START_RU.md'
    if doc.exists():entries.extend([(doc,'DOCS/FS37.MD'),(doc,'START.TXT'),(doc,'DRIVERS.TXT'),(doc,'MULTIOS/IMAGE.TXT')])
    for path,dest in entries:
     subprocess.run(['mcopy','-o','-i',drive,str(path),'::'+dest],check=True)
   with output.open('rb') as out:os.fsync(out.fileno())
  except BaseException:
   if made:output.unlink(missing_ok=True)
   raise
 return output
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--bundle',type=Path);p.add_argument('--efi',type=Path);a=p.parse_args()
 print(install(a.source,a.output,a.bundle,a.efi));print('UNPUBLISHED extended BIOS profile; BIOS EDD bootstrap; selected ATA/USB BOT block provider; NOT stable release')
