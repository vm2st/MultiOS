#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Create a NEW disposable 64 MiB MBR/BIOS/exFAT image, never a device.
Existing paths are refused. No partitioning/formatting of physical disks.
"""
import argparse, struct, pathlib, math, os, secrets
P16=lambda n:struct.pack('<H',n)
P32=lambda n:struct.pack('<I',n)
P64=lambda n:struct.pack('<Q',n)
def checksum(data, skip=()):
    n=0
    for i,b in enumerate(data):
        if i not in skip:n=(((n>>1)|((n&1)<<31))+b)&0xffffffff
    return n
def setcheck(b):
    c=0
    for i,x in enumerate(b):
        if i not in (2,3):c=(((c>>1)|((c&1)<<15))+x)&65535
    return c
def entry(name,first,size,flags=1,attr=32):
    raw=name.encode('utf-16le');l=len(raw)//2;n=2+(l+14)//15;b=bytearray(n*32)
    b[0]=0x85;b[1]=n-1;b[4:6]=P16(attr)
    for pos in (10,14,18):b[pos:pos+2]=P16(0x21)
    b[32]=0xc0;b[33]=flags;b[35]=l;h=0
    for x in name.upper().encode('utf-16le'):h=(((h>>1)|((h&1)<<15))+x)&65535
    b[36:38]=P16(h);b[40:48]=P64(size);b[52:56]=P32(first);b[56:64]=P64(size)
    for i in range((l+14)//15):
        b[(i+2)*32]=0xc1;chunk=raw[i*30:(i+1)*30];b[(i+2)*32+2:(i+2)*32+2+len(chunk)]=chunk
    b[2:4]=P16(setcheck(b));return b

def create(path,boot=None,kernel=None):
    path=pathlib.Path(path)
    if path.exists() or path.is_symlink():raise SystemExit('Refusing existing destination: '+str(path))
    total=131072;base=2048;length=total-base;fat=24;fatlen=256;heap=2048;spc=8;cs=4096;nc=(length-heap)//spc
    up=pathlib.Path(__file__).with_name('upcase.bin').read_bytes()
    bitmaplen=(nc+7)//8;bmfirst=10;upfirst=11;upcount=math.ceil(len(up)/cs);datafirst=upfirst+upcount
    msg=b'MultiOS 0.16-dev-unicode UNPUBLISHED / MultiPulti | vm2 studios | t.me/v2vproj\r\nType HELP for commands.\r\nWrites are experimental: use disposable images only.\r\n'
    mbr=bytearray(boot.read_bytes() if boot else b'\0'*510+b'\x55\xaa')
    if len(mbr)!=512:raise SystemExit('boot sector must be 512 bytes')
    mbr[440:444]=struct.pack('<I',secrets.randbits(32) or 1)
    mbr[446:462]=struct.pack('<B3sB3sII',0x80,b'\xfe\xff\xff',7,b'\xfe\xff\xff',base,length)
    k=kernel.read_bytes() if kernel else b''
    if len(k)>240*512:raise SystemExit('kernel too large')
    b=bytearray(512);b[:3]=b'\xeb\x76\x90';b[3:11]=b'EXFAT   '
    b[64:72]=P64(base);b[72:80]=P64(length);b[80:84]=P32(fat);b[84:88]=P32(fatlen);b[88:92]=P32(heap);b[92:96]=P32(nc);b[96:100]=P32(2);b[100:104]=P32(0x4d554c54);b[104:106]=P16(0x100);b[108]=9;b[109]=3;b[110]=1;b[111]=0x80;b[112]=255;b[510:512]=b'\x55\xaa'
    region=b
    for _ in range(8):region+=bytearray(508)+b'\0\0\x55\xaa'
    region+=bytearray(1024)
    region+=P32(checksum(region,(106,107,112)))*128
    ft=bytearray(fatlen*512);ft[:8]=P32(0xfffffff8)+P32(0xffffffff)
    def chain(first,count):
        for c in range(first,first+count):ft[c*4:c*4+4]=P32(c+1 if c<first+count-1 else 0xffffffff)
    chain(2,8);chain(bmfirst,1);chain(upfirst,upcount);chain(datafirst,1)
    bm=bytearray(cs)
    for c in range(2,datafirst+1):bm[(c-2)//8]|=1<<((c-2)%8)
    root=bytearray(cs*8)
    root[0]=0x83;label='MultiOS';root[1]=len(label);root[2:2+len(label)*2]=label.encode('utf-16le')
    root[32]=0x81;root[52:56]=P32(bmfirst);root[56:64]=P64(bitmaplen)
    root[64]=0x82;root[68:72]=P32(checksum(up));root[84:88]=P32(upfirst);root[88:96]=P64(len(up))
    fileentry=entry('WELCOME.TXT',datafirst,len(msg));root[96:96+len(fileentry)]=fileentry
    # Exclusive regular-file creation protects against accidental overwrite.
    fd=os.open(path,os.O_RDWR|os.O_CREAT|os.O_EXCL,0o600)
    with os.fdopen(fd,'w+b') as f:
        f.truncate(total*512)
        def write(sector,data):f.seek(sector*512);f.write(data)
        write(0,mbr);write(1,k);write(base,region);write(base+12,region);write(base+fat,ft)
        def cl(c,data):write(base+heap+(c-2)*spc,data)
        cl(2,root);cl(bmfirst,bm);cl(upfirst,up);cl(datafirst,msg);f.flush();os.fsync(f.fileno())
    print(f'Created {path}: BIOS+MBR, exFAT at LBA {base}, {nc} clusters, 64 MiB')
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=pathlib.Path);p.add_argument('--boot',type=pathlib.Path);p.add_argument('--kernel',type=pathlib.Path);a=p.parse_args()
    if bool(a.boot)!=bool(a.kernel):p.error('--boot and --kernel must be used together')
    create(a.output,a.boot,a.kernel)
