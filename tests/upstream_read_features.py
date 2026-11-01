#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — external NTFS fixture library, actual VFS reads.
from pathlib import Path
import ctypes as c,ctypes.util,subprocess,tempfile,hashlib,os,stat,sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from prepare_journal import prepare
from ntfs_fixture import populate
from ntfs_attrlist_fixture import populate_lists
r=Path(__file__).resolve().parents[1];checks=0

def command(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=180);assert p.returncode==0,(args,p.stdout[-1000:],p.stderr[-1000:]);return p.stdout

def probe(image,path,off,expected,size=None):
 global checks
 p=subprocess.run([str(Path(os.environ.get('MULTIOS_READ_PROBE',r/'build/fs-read-probe'))),str(image),path,str(off),str(len(expected))],capture_output=True,timeout=180)
 assert not p.returncode and p.stdout==expected,(path,off,p.returncode,p.stderr,p.stdout[:32])
 if size is not None:assert ('size='+str(size)+' ').encode() in p.stderr,p.stderr
 checks+=1

def advanced(path):
 lib=c.CDLL(ctypes.util.find_library('ntfs-3g'),use_errno=True);P=c.c_void_p;I=c.c_int;U=c.c_uint32;L=c.c_int64
 def fn(n,res,args):f=getattr(lib,n);f.restype=res;f.argtypes=args;return f
 mount=fn('ntfs_mount',P,[c.c_char_p,c.c_ulong]);unmount=fn('ntfs_umount',I,[P,I]);inode=fn('ntfs_pathname_to_inode',P,[P,P,c.c_char_p]);close=fn('ntfs_inode_close',I,[P]);closein=fn('ntfs_inode_close_in_dir',I,[P,P]);create=fn('ntfs_create',P,[P,U,P,c.c_uint8,U]);attr=fn('ntfs_attr_open',P,[P,U,P,U]);aclose=fn('ntfs_attr_close',None,[P]);write=fn('ntfs_attr_pwrite',L,[P,L,L,P]);pclose=fn('ntfs_attr_pclose',I,[P]);truncate=fn('ntfs_attr_truncate',I,[P,L]);setattr=fn('ntfs_set_ntfs_attrib',I,[P,P,c.c_size_t,I])
 def ok(x):
  if not x:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return x
 def good(x):
  if x<0:raise OSError(c.get_errno(),os.strerror(c.get_errno()))
  return x
 def name(s):return c.create_string_buffer(s.encode('utf-16le'))
 v=ok(mount(os.fsencode(path),0x08000000));root=ok(inode(v,None,b'/'))
 try:
  n=ok(create(root,0,name('huge-sparse.bin'),15,stat.S_IFREG));a=ok(attr(n,0x80,None,0));size=5*1024**3+4
  good(truncate(a,size));assert good(write(a,size-4,4,c.create_string_buffer(b'END!')))==4;aclose(a);good(closein(n,root))
  d=ok(create(root,0,name('COMPRESSED'),10,stat.S_IFDIR));flags=c.create_string_buffer((0x800).to_bytes(4,'little'));good(setattr(d,flags,4,0))
  n=ok(create(d,0,name('lznt1.bin'),9,stat.S_IFREG));a=ok(attr(n,0x80,None,0));payload=b'COMPRESSED-NTFS\n'*8192
  assert good(write(a,0,len(payload),c.create_string_buffer(payload)))==len(payload);good(pclose(a));aclose(a);good(closein(n,d));good(closein(d,root))
 finally:good(close(root));good(unmount(v,0))
 return size,payload
with tempfile.TemporaryDirectory(prefix='read-features-',dir=r/'build') as td:
 td=Path(td);part=td/'ntfs.part'
 with part.open('wb') as f:f.truncate(96*1024*1024)
 command('mkntfs','-F','-Q','-s','512','-c','4096',part);populate(part);populate_lists(part,fragment_runs=80);size,compressed=advanced(part)
 before=hashlib.sha256(part.read_bytes()).digest()
 probe(part,'/resident.txt',0,b'Resident NTFS\n');probe(part,'/hardlink.txt',0,b'Resident NTFS\n')
 probe(part,'/DOCS/Nested.txt',0,b'Nested NTFS directory\n');probe(part,'/Unicode_привет.txt',0,b'Unicode metadata\n')
 probe(part,'/sparse.bin',0,bytes(4096));probe(part,'/sparse.bin',1048576-16,bytes(6)+b'SPARSE-END');probe(part,'/uninitialized.bin',0,bytes(16384))
 expected=b''.join(bytes([i%251])*4096 for i in range(80));probe(part,'/split.bin',0,expected)
 assert command('ntfscat',part,'/split.bin')==expected
 probe(part,'/huge-sparse.bin',size-20,bytes(16)+b'END!',size);probe(part,'/huge-sparse.bin',2**32+19,bytes(8192),size)
 probe(part,'/COMPRESSED/lznt1.bin',0,compressed);assert command('ntfscat',part,'/COMPRESSED/lznt1.bin')==compressed
 assert hashlib.sha256(part.read_bytes()).digest()==before,'RO path modified NTFS source'
 command('ntfsfix','-n',part)
 # Only a protected disposable MBR image can exercise MultiOS mutations.
 source=td/'source.img';image=td/'protected.img';m=bytearray(512);m[510:]=b'\x55\xaa';m[446:462]=struct.pack('<B3sB3sII',0,bytes(3),7,bytes(3),2048,part.stat().st_size//512)
 with source.open('wb') as f:f.write(m);f.seek(2048*512);f.write(part.read_bytes())
 prepare(source,image,1,experimental_ntfs=True,journal_records=4096)
 print(command(Path(os.environ.get('MULTIOS_MUTATION_PROBE',r/'build/fs-mutation-probe')),image).decode().strip())
 print(command(Path(os.environ.get('MULTIOS_RANDOM_PROBE',r/'build/fs-random-probe')),image,'compressed').decode().strip())
 print(command(Path(os.environ.get('MULTIOS_STREAM_PROBE',r/'build/ntfs-stream-probe')),image,'compressed').decode().strip())
 with image.open('rb') as f:f.seek(2048*512);part.write_bytes(f.read(part.stat().st_size))
 updated=bytes(i%17 for i in range(32768))*2
 assert command('ntfscat',part,'/COMPRESSED/lznt1.bin')==updated
 probe(part,'/huge-sparse.bin',size-4,b'END!MORE',size+4);command('ntfsfix','-n',part)
 print(f'PASS {checks} advanced NTFS read probes: resident/hardlink, nested/Unicode, sparse/uninitialized, 80 interleaved allocation chunks + ATTRIBUTE_LIST, sparse >5 GiB, compressed LZNT1; external ntfscat comparisons; RO source SHA256 unchanged.')
