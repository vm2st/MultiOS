#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Edit a powered-off COPY of the unified image, never a physical device.
Needs Python 3, mtools and fsck.fat/dosfstools (Linux/WSL). Updates are copy-on-write + atomic rename.
C files do not become executable drivers merely by copying them into the image.
"""
import argparse, contextlib, fcntl, os, shutil, stat, struct, subprocess, tempfile
from pathlib import Path
CREDIT='vm2 studios | t.me/v2vproj'
FAT_START=2048; FAT_SECTORS=131072; DATA_START=133120; KERNEL_BYTES=240*512
MAX_IMAGE=512*1024*1024

def validate(f):
    size=os.fstat(f.fileno()).st_size
    if size<DATA_START*512+24*512 or size>MAX_IMAGE or size%512: raise ValueError('Not a supported unified image size')
    f.seek(0);m=f.read(512)
    if m[432:440]!=b'MP11\xf0\x00\x01\x00':raise ValueError('Expected compatible MP11 boot bundle; rebuild incompatible images before use')
    if m[510:512]!=b'\x55\xaa' or not any(m[440:444]) or any(m[478:510]):raise ValueError('Invalid MBR identity/unused partition entries')
    expected=[(0xef,FAT_START,FAT_SECTORS),(m[466],DATA_START,size//512-DATA_START)]
    if m[466] not in (7,0x0b,0x0c):raise ValueError('Unsupported data partition type')
    for i,(kind,start,length) in enumerate(expected):
        row=m[446+i*16:462+i*16];actual=(row[4],*struct.unpack_from('<II',row,8))
        if actual!=(kind,start,length):raise ValueError('Not the MultiOS unified layout (no raw device support)')
    f.seek(FAT_START*512);fat=f.read(512)
    f.seek(DATA_START*512);exfat=f.read(512)
    data_ok=exfat[3:11] in (b'EXFAT   ',b'NTFS    ') or (exfat[82:90]==b'FAT32   ' and exfat[11:13]==b'\x00\x02')
    if fat[82:90]!=b'FAT32   ' or not data_ok:raise ValueError('Invalid filesystem signatures')
    return size

@contextlib.contextmanager
def opened(path,write=False):
    fd=os.open(path,(os.O_RDWR if write else os.O_RDONLY)|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,'r+b' if write else 'rb') as f:
        s=os.fstat(f.fileno())
        if not stat.S_ISREG(s.st_mode) or s.st_nlink!=1:raise ValueError('Only a non-symlink, non-hardlinked regular image file is accepted')
        fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)
        # Also participate in POSIX record locks used by QEMU on Linux.
        fcntl.lockf(f,(fcntl.LOCK_EX if write else fcntl.LOCK_SH)|fcntl.LOCK_NB)
        validate(f); yield f,s

def fatarg(path):return str(path)+'@@'+str(FAT_START*512)
def relpath(value):
    v=value.replace('\\','/').lstrip('/')
    if not v or ':' in v or any(p in ('','.','..') for p in v.split('/')):raise ValueError('Invalid image-relative path')
    if any(ord(c)<32 or ord(c)>126 or c in '*?[]' for c in v):raise ValueError('Only literal ASCII paths; no wildcards')
    return v

def extract(image,rel,dest):
    subprocess.run(['mcopy','-n','-i',fatarg(image),'::'+rel,str(dest)],check=True,stdout=subprocess.DEVNULL)

def source_bytes(path,limit):
    # Nonblocking open rejects FIFOs before a read can hang. Snapshot once, then
    # validate and inject exactly those bytes (no validated-source reread race).
    fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    with os.fdopen(fd,'rb') as f:
        before=os.fstat(f.fileno())
        if not stat.S_ISREG(before.st_mode) or before.st_size>limit:raise ValueError('Source must be a regular file within its size limit')
        data=f.read(limit+1)
        after=os.fstat(f.fileno())
        if len(data)>limit or len(data)!=before.st_size or (before.st_size,before.st_mtime_ns,before.st_ctime_ns)!=(after.st_size,after.st_mtime_ns,after.st_ctime_ns):raise ValueError('Source changed while being read')
        return data

def inject_data(image,rel,data):
    # Never let mtools write through a corrupted FAT chain into the exFAT
    # partition or raw kernel. Work on an isolated, fixed-size ESP snapshot.
    with tempfile.TemporaryDirectory(prefix='multios-source-') as td:
        snapshot=Path(td)/'snapshot';snapshot.write_bytes(data)
        esp=Path(td)/'esp.fat'
        with Path(image).open('rb') as f,esp.open('wb') as target:
            f.seek(FAT_START*512)
            left=FAT_SECTORS*512
            while left:
                chunk=f.read(min(left,1024*1024))
                if not chunk:raise ValueError('Truncated ESP')
                target.write(chunk);left-=len(chunk)
        subprocess.run(['fsck.fat','-n',str(esp)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['mcopy','-o','-i',str(esp),str(snapshot),'::'+rel],check=True)
        if esp.stat().st_size!=FAT_SECTORS*512:raise ValueError('ESP editor changed partition size')
        subprocess.run(['fsck.fat','-n',str(esp)],check=True,stdout=subprocess.DEVNULL)
        with Path(image).open('r+b') as f,esp.open('rb') as source:
            f.seek(FAT_START*512);shutil.copyfileobj(source,f)


def inject(image,rel,source):
    inject_data(image,rel,source_bytes(source,64*1024*1024))

def kernel_bytes(path):
    b=source_bytes(path,KERNEL_BYTES)
    if not 16<=len(b)<=KERNEL_BYTES or b.startswith((b'\x7fELF',b'MZ')):raise ValueError('Expected freestanding raw i386 kernel.bin, 16..122880 bytes, not ELF/EFI')
    return b

def efi_bytes(path):
    b=source_bytes(path,16*1024*1024)
    if len(b)<256 or len(b)>16*1024*1024 or b[:2]!=b'MZ':raise ValueError('Expected a built x64 EFI application')
    pe=struct.unpack_from('<I',b,60)[0]
    if pe+96>len(b) or b[pe:pe+4]!=b'PE\0\0' or struct.unpack_from('<H',b,pe+4)[0]!=0x8664 or struct.unpack_from('<H',b,pe+24)[0]!=0x20b or struct.unpack_from('<H',b,pe+92)[0]!=10:
        raise ValueError('Not an x86-64 PE32+ EFI application')
    return b

def boot_bytes(path):
    b=source_bytes(path,512)
    if len(b)!=512 or b[510:512]!=b'\x55\xaa' or any(b[440:510]):
        raise ValueError('Expected build/boot.bin template: 512 bytes, blank disk ID/table, valid signature')
    return b

def update_boot(image,source):
    b=boot_bytes(source)
    inject_data(image,'MULTIOS/BOOT32.BIN',b)
    # Preserve disk identity, partition table and signature (440..511).
    with image.open('r+b') as f:f.seek(0);f.write(b[:440]);f.flush();os.fsync(f.fileno())

def update_kernel(image,source):
    b=kernel_bytes(source)
    inject_data(image,'MULTIOS/KERNEL32.BIN',b)
    with image.open('r+b') as f:f.seek(512);f.write(b+bytes(KERNEL_BYTES-len(b)));f.flush();os.fsync(f.fileno())

def transaction(path,edit):
    path=Path(os.path.abspath(path))
    with opened(path,True) as (original,s):
        fd,temp=tempfile.mkstemp(prefix='.'+path.name+'.edit-',dir=path.parent);temp=Path(temp)
        try:
            with os.fdopen(fd,'wb') as target:
                original.seek(0);shutil.copyfileobj(original,target)
            edit(temp)
            with temp.open('r+b') as f:validate(f);os.fsync(f.fileno())
            now=os.stat(path,follow_symlinks=False)
            if (now.st_dev,now.st_ino,now.st_size,now.st_mtime_ns,now.st_ctime_ns,now.st_nlink)!=(s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns,1):raise ValueError('Original image changed concurrently; no replacement')
            os.chmod(temp,stat.S_IMODE(s.st_mode));os.replace(temp,path)
            d=os.open(path.parent,os.O_RDONLY|os.O_DIRECTORY)
            try:os.fsync(d)
            finally:os.close(d)
        finally:
            if temp.exists():temp.unlink()
    print('Saved atomically: '+str(path)+' | '+CREDIT)

def config_text(text,key,value):
    allowed={'output','input','block','power','osk','output.options','input.options','block.options','power.options'}
    if key not in allowed or any(ord(c)<32 or ord(c)>126 for c in value) or len(value)>127:raise ValueError('Invalid config key/value')
    if key=='osk' and value not in ('auto','on','off'):raise ValueError('osk must be auto/on/off')
    if key in ('output','input','block','power') and (not value or len(value)>39 or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-_.' for c in value)):raise ValueError('Invalid module ID')
    lines=text.splitlines();found=False;out=[]
    for line in lines:
        if line.strip().startswith(('#',';')):out.append(line);continue
        if '=' in line and line.split('=',1)[0].strip()==key:
            if found:raise ValueError('Duplicate key in existing config')
            out.append(key+'='+value);found=True
        else:out.append(line)
    if not found:out.append(key+'='+value)
    result='\n'.join(out)+'\n'
    if len(result.encode('ascii'))>2048:raise ValueError('Config too large')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_subparsers(dest='command',required=True)
    p=commands.add_parser('info');p.add_argument('image',type=Path)
    p=commands.add_parser('get');p.add_argument('image',type=Path);p.add_argument('path');p.add_argument('output',type=Path)
    p=commands.add_parser('put');p.add_argument('image',type=Path);p.add_argument('path');p.add_argument('source',type=Path)
    p=commands.add_parser('set');p.add_argument('image',type=Path);p.add_argument('key');p.add_argument('value')
    p=commands.add_parser('install');p.add_argument('image',type=Path);p.add_argument('--kernel',type=Path,required=True);p.add_argument('--efi',type=Path,required=True);p.add_argument('--boot',type=Path)
    args=parser.parse_args()
    if args.command=='info':
        with opened(args.image) as (f,_):
            print('MultiOS / MultiPulti | '+CREDIT);print('Image bytes:',validate(f));print('BIOS: own i386 kernel, IDE only. UEFI x64: firmware live app.')
            subprocess.run(['mdir','-i',fatarg(args.image),'::MULTIOS'],check=True)
        return
    if args.command=='get':
        rel=relpath(args.path)
        if args.output.exists() or args.output.is_symlink():raise ValueError('Refusing existing output')
        with opened(args.image):
            with tempfile.TemporaryDirectory(prefix='multios-get-') as td:
                temp=Path(td)/'file';extract(args.image,rel,temp)
                with args.output.open('xb') as out,temp.open('rb') as src:shutil.copyfileobj(src,out)
        print('Extracted: '+str(args.output));return
    def edit(temp):
        if args.command=='install':
            efi_bytes(args.efi);kernel_bytes(args.kernel)
            if args.boot is not None: boot_bytes(args.boot);update_boot(temp,args.boot)
            update_kernel(temp,args.kernel);inject_data(temp,'EFI/BOOT/BOOTX64.EFI',efi_bytes(args.efi))
        elif args.command=='put':
            rel=relpath(args.path)
            if not args.source.is_file() or args.source.is_symlink() or args.source.stat().st_size>64*1024*1024:raise ValueError('Source must be a regular file <=64 MiB')
            if rel.upper()=='MULTIOS/BOOT32.BIN':update_boot(temp,args.source)
            elif rel.upper()=='MULTIOS/KERNEL32.BIN':update_kernel(temp,args.source)
            else:
                if rel.upper()=='EFI/BOOT/BOOTX64.EFI':inject_data(temp,rel,efi_bytes(args.source))
                else:inject(temp,rel,args.source)
        else:
            with tempfile.TemporaryDirectory(prefix='multios-cfg-') as td:
                cfg=Path(td)/'BOOT.CFG';extract(temp,'MULTIOS/BOOT.CFG',cfg)
                cfg.write_text(config_text(cfg.read_text(encoding='ascii'),args.key,args.value),encoding='ascii')
                inject(temp,'MULTIOS/BOOT.CFG',cfg)
    transaction(args.image,edit)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,UnicodeError,subprocess.CalledProcessError) as e:raise SystemExit('imagectl: '+str(e))
