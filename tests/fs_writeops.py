#!/usr/bin/env python3
"""Real write-semantics regressions on ordinary disposable image copies.
Usage: fs_writeops.py IMAGES [--prepare-only] [--before]
Requires mtools, dosfstools, exfatprogs, ntfs-3g and its shared library.
"""
from pathlib import Path
import subprocess,struct,sys,shutil,ctypes as c,ctypes.util,os
root=Path(__file__).resolve().parents[1];work=root/'build/writeops31';work.mkdir(parents=True,exist_ok=True)
def run(args,ok=True):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600)
 if ok:assert not p.returncode,(args,p.returncode,p.stdout[-2000:],p.stderr[-2000:])
 return p

def protect_ntfs(part):
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int
 def fn(name,ret,args):
  f=getattr(lib,name);f.restype=ret;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);umount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);close=fn('ntfs_inode_close',I,[P]);get=fn('ntfs_get_ntfs_attrib',I,[P,P,c.c_size_t]);setflags=fn('ntfs_set_ntfs_attrib',I,[P,P,c.c_size_t,I])
 v=mount(os.fsencode(part),0x08000000);assert v
 ni=inode(v,None,b'/ReadOnly.txt');assert ni;flags=c.create_string_buffer(4);assert get(ni,flags,4)>=0
 value=c.create_string_buffer((int.from_bytes(flags.raw,'little')|5).to_bytes(4,'little'));assert not setflags(ni,value,4,0);assert not close(ni);assert not umount(v,0)

def protect_exfat(data):
 # Seed root fits one cluster. Locate an exact file-entry set, then recompute
 # its exFAT SetChecksum. This is a fixture writer, not a production FS repair.
 heap,cluster=struct.unpack_from('<I',data,88)[0],struct.unpack_from('<I',data,96)[0]
 spc=1<<data[109];start=(heap+(cluster-2)*spc)*512;end=start+spc*512;found=0
 for pos in range(start,end,32):
  if data[pos]!=0x85:continue
  count=data[pos+1];assert pos+(count+1)*32<=end
  name=bytearray()
  for j in range(1,count+1):
   q=pos+j*32
   if data[q]==0xc1:name.extend(data[q+2:q+32])
  if name.decode('utf-16le').rstrip('\0')!='ReadOnly.txt':continue
  struct.pack_into('<H',data,pos+4,struct.unpack_from('<H',data,pos+4)[0]|5)
  checksum=0
  for j,b in enumerate(data[pos:pos+(count+1)*32]):
   if j in (2,3):continue
   checksum=(((checksum&1)<<15)+(checksum>>1)+b)&65535
  struct.pack_into('<H',data,pos+2,checksum);found+=1
 assert found==1,found

if __name__=='__main__':
 images=Path(sys.argv[1]);cases=[]
 for fs in ('fat32','exfat','ntfs'):
  found=list(images.glob('*-usb-'+fs+'.img'));assert len(found)==1 and found[0].is_file() and not found[0].is_symlink()
  image=work/(fs+'.img');shutil.copyfile(found[0],image);run([root/'build/fs-writeops-probe',image,'seed'])
  raw=bytearray(image.read_bytes());start,count=struct.unpack_from('<II',raw,470);part=work/(fs+'.part');part.write_bytes(raw[start*512:(start+count)*512])
  if fs=='fat32':run(['mattrib','-i',part,'+r','+s','::ReadOnly.txt'])
  elif fs=='ntfs':protect_ntfs(part)
  else:
   data=bytearray(part.read_bytes());protect_exfat(data);part.write_bytes(data)
  raw[start*512:(start+count)*512]=part.read_bytes();image.write_bytes(raw);cases.append((fs,image))
 if '--prepare-only' in sys.argv:print('Prepared 3 seeded, read-only-source fixture images');sys.exit(0)
 for fs,base in cases:
  for binary in (('fs-writeops-probe',) if '--before' in sys.argv else ('fs-writeops-probe','fs-writeops-probe-sanitize')):
   for command in ('case','copy-ro','guards'):
    image=work/'working.img';shutil.copyfile(base,image);p=run([root/'build'/binary,image,command],ok='--before' not in sys.argv)
    print(fs+'/'+binary+'/'+command+': status='+str(p.returncode)+' '+p.stdout.decode().strip(),flush=True)
    if '--before' not in sys.argv:
     raw=image.read_bytes();start,count=struct.unpack_from('<II',raw,470);part=work/'verify.part';part.write_bytes(raw[start*512:(start+count)*512])
     checker='fsck.fat' if fs=='fat32' else 'fsck.exfat' if fs=='exfat' else 'ntfsfix';run([checker,'-n',part])
     if fs in ('fat32','ntfs'):
      for path in ('ReadOnly.txt','CopiedReadOnly.txt') if command=='copy-ro' else ('ReadOnly.txt',):
       args=['mtype','-i',part,'::'+path] if fs=='fat32' else ['ntfscat',part,'/'+path]
       expected=bytearray((i*37+11)&255 for i in range(8193))
       if path=='CopiedReadOnly.txt':expected[509:514]=b'LOCAL'
       assert run(args).stdout==expected,(fs,path)
     print('PASS independent '+fs+' read-only checker'+(' and exact source/copy bytes' if fs!='exfat' else ''),flush=True)
 print('Completed write-semantics probes' +(' (baseline failures expected)' if '--before' in sys.argv else ': PASS'))
