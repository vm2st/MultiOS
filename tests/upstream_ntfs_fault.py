#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — reproducible NTFS power-failure matrices.
from pathlib import Path
import sys,subprocess,tempfile,struct,os,ctypes as c,ctypes.util,stat
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));from prepare_journal import prepare
compressed=bool(os.environ.get('MULTIOS_TEST_COMPRESSED'));tag=('compressed-' if compressed else '')
capacity=int(os.environ.get('MULTIOS_TEST_JOURNAL_RECORDS','128'));env=dict(os.environ,MULTIOS_NTFS_TEST_TIME='1')
def compressed_seed(path):
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;U=c.c_uint32;L=c.c_int64
 def fn(n,res,args):f=getattr(lib,n);f.restype=res;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);unmount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);close=fn('ntfs_inode_close',I,[P]);closein=fn('ntfs_inode_close_in_dir',I,[P,P]);create=fn('ntfs_create',P,[P,U,P,c.c_uint8,U]);attr=fn('ntfs_attr_open',P,[P,U,P,U]);aclose=fn('ntfs_attr_close',None,[P]);write=fn('ntfs_attr_pwrite',L,[P,L,L,P]);pclose=fn('ntfs_attr_pclose',I,[P]);getflags=fn('ntfs_get_ntfs_attrib',I,[P,P,c.c_size_t]);setflags=fn('ntfs_set_ntfs_attrib',I,[P,P,c.c_size_t,I])
 v=mount(os.fsencode(path),0x08000000);assert v;root=inode(v,None,b'/');assert root
 flags=c.create_string_buffer(4);assert getflags(root,flags,4)>=0;original=bytes(flags.raw);value=c.create_string_buffer((int.from_bytes(original,'little')|0x800).to_bytes(4,'little'));assert not setflags(root,value,4,0)
 name=c.create_string_buffer('old.txt'.encode('utf-16le'));ni=create(root,0,name,7,stat.S_IFREG);assert ni;a=attr(ni,0x80,None,0);assert a;data=b'COMPRESSION-FAULT'*512;assert write(a,0,len(data),c.create_string_buffer(data))==len(data);assert not pclose(a);aclose(a);assert not closein(ni,root)
 value=c.create_string_buffer(original);assert not setflags(root,value,4,0);assert not close(root);assert not unmount(v,0)
 p=subprocess.run(['ntfscat',str(path),'/old.txt'],capture_output=True);assert not p.returncode and p.stdout==data

with tempfile.TemporaryDirectory(prefix='ntfs-fault-',dir=r/'build') as td:
 td=Path(td);part=td/'ntfs.part';source=td/'source.img';image=td/'test.img'
 with part.open('wb') as f:f.truncate(64*1024*1024)
 subprocess.run(['mkntfs','-F','-Q','-s','512','-c','4096',str(part)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
 if compressed:compressed_seed(part)
 m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),7,bytes(3),2048,part.stat().st_size//512)
 with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
 prepare(source,image,1,experimental_ntfs=True,journal_records=capacity)
 p=subprocess.run([str(r/'build/multios-host'),str(image),'--allow-write'],input=(b'unlock YES\nmd empty\nlock\nexit\n' if compressed else b'unlock YES\nwrite old.txt "original"\nmd empty\nlock\nexit\n'),capture_output=True,env=env);assert not p.returncode and b'Error:' not in p.stdout,(p.stdout,p.stderr)
 for binary in ('storage-tx-test','storage-tx-sanitize'):
  p=subprocess.run([str(r/'build'/binary),str(image)],capture_output=True,timeout=600,env=env)
  text=p.stdout.decode(errors='replace');(r/'build'/('unified-ntfs-'+tag+str(capacity)+'-'+binary+'.log')).write_text(text+p.stderr.decode(errors='replace'))
  assert not p.returncode and 'PASS real-FS private transactions:' in text,(p.returncode,text[-2000:],p.stderr[-2000:])
  print(binary+': '+text.splitlines()[-1],flush=True)
print('PASS NTFS '+tag+'fault matrices, journal records='+str(capacity)+'. Whole NTFS-3G library instrumented in sanitizer binary; model, not physical power-cut certification.')
