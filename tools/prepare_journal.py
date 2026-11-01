#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — copy-only reserved-tail journal preparation.
"""Create a NEW disposable MBR image with a private undo tail. NTFS requires explicit experimental opt-in;
this preparation tool does not implement filesystem writing itself.
Never modifies the source; rejects devices, symlinks, hardlinks, existing output,
extended/GPT tables, overlaps, unsupported volume types and geometry mismatch.
Use only powered-off image COPIES. No in-place or physical-drive mode exists.
"""
import argparse, os, stat, struct, zlib, secrets, fcntl
from pathlib import Path
S=512; J=258; LIMIT=0x10000000
U32=lambda b,o:struct.unpack_from('<I',b,o)[0]
U64=lambda b,o:struct.unpack_from('<Q',b,o)[0]
def crc(b):return zlib.crc32(b)&0xffffffff
def put(b,o,fmt,v):struct.pack_into('<'+fmt,b,o,v)
def header(total,start,data,n,media,records=128):
 jsectors=2+2*records
 b=bytearray(S);b[:8]=b'MPUNDO1\0'
 for o,v in [(8,1),(12,512),(16,512),(20,0),(40,0),(44,records)]:put(b,o,'I',v)
 for o,v in [(24,1),(32,0),(48,start),(56,jsectors),(64,data),(72,n),(80,total)]:put(b,o,'Q',v)
 b[88:104]=media;put(b,508,'I',crc(b[:508]));return b

def prepare(source,output,partition,*,experimental_ntfs=False,journal_records=128):
 if type(journal_records) is not int or not 1<=journal_records<=4096:raise ValueError("journal_records must be 1..4096")
 J=2+2*journal_records
 if not 1<=partition<=4:raise ValueError('Partition must be 1..4')
 source=Path(source);output=Path(output)
 fd=os.open(source,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
 made=False
 try:
  with os.fdopen(fd,'rb',closefd=True) as src:
   fcntl.flock(src,fcntl.LOCK_SH|fcntl.LOCK_NB)
   # Also conflicts with the POSIX harness's fcntl lock.
   fcntl.lockf(src,fcntl.LOCK_SH|fcntl.LOCK_NB)
   info=os.fstat(src.fileno())
   if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size%S or info.st_size<2048*S:raise ValueError('Only regular sector-aligned single-link images >=1 MiB')
   old=info.st_size//S;total=old+J+2
   if total>LIMIT:raise ValueError('Image exceeds current HAL limit')
   src.seek((old-2)*S);tail=src.read(2*S)
   if tail[:8]==b'MPSTORE1' or tail[S:S+8]==b'MPSTORE1':raise ValueError('Already prepared: use original unprepared image')
   src.seek(0);mbr=src.read(S)
   if mbr[510:]!=b'\x55\xaa':raise ValueError('MBR signature missing')
   parts=[]
   for i in range(4):
    p=mbr[446+i*16:462+i*16];start=U32(p,8);n=U32(p,12);t=p[4]
    if not t:
     if start or n:raise ValueError('Malformed empty partition')
     parts.append((0,0,0));continue
    if t in (5,0x0f,0x85,0xee,0xda) or not start or not n or start>=old or n>old-start:raise ValueError('Unsupported/out-of-bounds MBR partition')
    if any(k and start<a+k and a<start+n for a,k,_ in parts):raise ValueError('Overlapping partitions')
    parts.append((start,n,t))
   data,n,t=parts[partition-1]
   if not t:raise ValueError('Selected partition is empty')
   src.seek(data*S);boot=src.read(S)
   if boot[3:11]==b'EXFAT   ':
    if boot[108]!=9 or U64(boot,64)!=data or U64(boot,72)!=n:raise ValueError('exFAT geometry differs from partition')
    kind='exFAT'
   elif t in (0x0b,0x0c) and struct.unpack_from('<H',boot,11)[0]==512 and not struct.unpack_from('<H',boot,17)[0] and U32(boot,36) and U32(boot,32)==n:
    kind='FAT32'
   elif experimental_ntfs and t==7 and boot[3:11]==b'NTFS    ' and struct.unpack_from('<H',boot,11)[0]==512 and U64(boot,40)+1==n:
    kind='NTFS experimental'
   else:raise ValueError('Only FAT32/exFAT by default; NTFS requires explicit experimental_ntfs opt-in.')
   slots=[i for i,(_,n,t) in enumerate(parts) if not n and not t]
   if not slots:raise ValueError('One empty MBR slot is required for the reserved journal partition')
   mbr=bytearray(mbr);pos=446+slots[0]*16
   mbr[pos:pos+16]=struct.pack('<B3sB3sII',0,b'\0'*3,0xda,b'\0'*3,old,J+2)
   media=secrets.token_bytes(16)
   if media==bytes(16):raise ValueError('Invalid random media ID')
   h=header(total,old,data,n,media,journal_records)
   footer=bytearray(S);footer[:8]=b'MPSTORE1';put(footer,8,'I',1);put(footer,12,'I',S)
   for o,v in [(16,total),(24,old),(32,J),(40,data),(48,n)]:put(footer,o,'Q',v)
   footer[56:72]=media;put(footer,72,'I',partition);put(footer,76,'I',crc(mbr));put(footer,508,'I',crc(footer[:508]))
   outfd=os.open(output,os.O_RDWR|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600);made=True
   with os.fdopen(outfd,'w+b') as dst:
    src.seek(0);left=info.st_size
    while left:
     b=src.read(min(left,1024*1024))
     if not b:raise ValueError('Source changed/truncated during copy')
     if dst.write(b)!=len(b):raise OSError('Short output write')
     left-=len(b)
    after=os.fstat(src.fileno())
    if (info.st_size,info.st_mtime_ns,info.st_ctime_ns)!=(after.st_size,after.st_mtime_ns,after.st_ctime_ns):raise ValueError('Source changed during copy')
    dst.write(h);dst.write(h);dst.write(bytes((J-2)*S));dst.write(footer);dst.write(footer)
    dst.seek(0);dst.write(mbr);dst.flush();os.fsync(dst.fileno())
   # Persist new directory entry as well as file contents on supported POSIX hosts.
   dfd=os.open(output.parent,os.O_RDONLY|os.O_DIRECTORY)
   try:os.fsync(dfd)
   finally:os.close(dfd)
   return kind,total
 except BaseException:
  if made:
   try:output.unlink()
   except FileNotFoundError:pass
  raise
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);p.add_argument('--partition',type=int,required=True);p.add_argument('--experimental-ntfs',action='store_true');p.add_argument('--journal-records',type=int,default=128);a=p.parse_args()
 try:
  kind,total=prepare(a.source,a.output,a.partition,experimental_ntfs=a.experimental_ntfs,journal_records=a.journal_records)
  print(f'Created NEW {kind} test image, {total} sectors; private {a.journal_records}-sector transaction budget. UNPUBLISHED; experimental, not stable RW.')
 except (ValueError,OSError) as e:p.exit(1,str(e)+'\n')
