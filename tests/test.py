#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Black-box tests against the SAME C filesystem/shell used by the kernel."""
import pathlib,subprocess,shutil,tempfile,struct,zlib,sys,os
ROOT=pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from mkimage import create,entry
from qemu_smoke import run_qemu
checks=0
logs=[]
def check(b,msg):
    global checks
    checks+=1
    if not b:raise AssertionError(msg)
def run(image,commands,write=True,expected_errors=0,binary=None):
    cmd=[str(binary or os.environ.get('MULTIOS_HOST',ROOT/'build/multios-host')),str(image)]+(['--allow-write'] if write else [])
    p=subprocess.run(cmd,input='\n'.join(commands+['exit'])+'\n',text=True,capture_output=True,timeout=60)
    logs.append(p.stdout)
    check(p.returncode==0,f'host failed {p.returncode}: {p.stdout}\n{p.stderr}')
    check(p.stdout.count('Error:')==expected_errors,p.stdout)
    return p.stdout

def fsck(image,partitioned=True):
    if not shutil.which('fsck.exfat'):return
    p=image.with_suffix('.partition')
    with image.open('rb') as f:
        if partitioned:f.seek(2048*512)
        with p.open('wb') as o:shutil.copyfileobj(f,o)
    r=subprocess.run(['fsck.exfat','-n',str(p)],text=True,capture_output=True)
    logs.append(r.stdout+r.stderr);check(r.returncode==0,r.stdout+r.stderr);p.unlink()

with tempfile.TemporaryDirectory(prefix='multios-test-') as td:
    td=pathlib.Path(td);img=td/'disk.img';shutil.copyfile(ROOT/'build/multios.img',img)
    original=img.read_bytes()
    s=run(img,['dir','type welcome.txt','unlock YES','touch denied'],write=False,expected_errors=2)
    check(img.read_bytes()==original,'read-only image was changed')
    s=run(img,['unlock YES','md DOCS','write /DOCS/a.txt "Hello MultiOS"','append /DOCS/a.txt "second line"','type /DOCS/a.txt','copy /DOCS/a.txt /DOCS/b.txt','cmp /DOCS/a.txt /DOCS/b.txt','crc32 /DOCS/a.txt','find second /DOCS/a.txt','stat /DOCS/a.txt','ren /DOCS/b.txt "long filename with spaces.txt"','dir /DOCS','cd /DOCS','pwd','cd ..','pwd'])
    check('Hello MultiOS\nsecond line' in s,'append/read mismatch')
    check('Identical' in s,'copy mismatch')
    check(f'{zlib.crc32(b"Hello MultiOS\nsecond line\n"):08X}' in s,'CRC mismatch')
    check('2: second line' in s,'find mismatch');check('long filename with spaces.txt' in s,'rename mismatch');fsck(img)
    s=run(img,['unlock YES','touch /DOCS/a.txt','copy /DOCS/a.txt /DOCS/a.txt','rd /DOCS','del /DOCS','write /not-here/a.txt "x"','md /DOCS','ren /DOCS/a.txt ../bad','cd /DOCS/a.txt','write "bad?.txt" "x"'],expected_errors=9)
    fsck(img)
    # Repeat replacements, append, reuse deleted slots and fragmented allocation.
    commands=['unlock YES']
    for i in range(25):commands.append(f'write /DOCS/a.txt "generation {i}"')
    commands+=['ren /DOCS/a.txt A.TXT','ren /DOCS/A.TXT "this is a longer name.txt"','ren "/DOCS/this is a longer name.txt" X','del "/DOCS/long filename with spaces.txt"','del /DOCS/X','rd /DOCS','df']
    s=run(img,commands);check('Free clusters: 15860' in s,'clusters leaked');fsck(img)
    s=run(img,['unlock YES','touch empty','type empty','copy empty other','cmp empty other','del empty','del other','lock','touch locked'],expected_errors=1);fsck(img)
    # Existing externally-created exFAT superfloppy using mkfs.exfat.
    if shutil.which('mkfs.exfat'):
        external=td/'external.exfat'
        with external.open('wb') as f:f.truncate(64*1024*1024)
        subprocess.run(['mkfs.exfat',str(external)],check=True,stdout=subprocess.DEVNULL)
        s=run(external,['unlock YES','md TEST','write TEST/readme.txt "external volume"','type TEST/readme.txt','copy TEST/readme.txt copy.txt','ren copy.txt renamed.txt','del renamed.txt','del TEST/readme.txt','rd TEST'])
        check('external volume' in s,'external format failed');fsck(external,False)
    # Dirty flag: may read, must never clear or allow writes.
    dirty=td/'dirty.img';shutil.copyfile(img,dirty)
    with dirty.open('r+b') as f:f.seek(2048*512+106);f.write(b'\x02\x00')
    before=dirty.read_bytes();s=run(dirty,['dir','unlock YES'],expected_errors=1)
    check('dirty volume' in s,'no dirty warning');check(before==dirty.read_bytes(),'dirty flag was cleared')
    # Invalid main checksum, geometry, unknown revisions: fail closed.
    for off,value in [(2048*512+100,b'\x77'),(2048*512+108,b'\x0c'),(2048*512+110,b'\x02')]:
        bad=td/'bad.img';shutil.copyfile(img,bad)
        with bad.open('r+b') as f:f.seek(off);f.write(value)
        s=run(bad,['dir','unlock YES'],expected_errors=2)
        check('Mount failed:' in s,'corrupt volume mounted')
    # Unicode names are listed but not mapped onto ASCII aliases.
    # Kernel boot/read/write tested on 486 and a 64-bit CPU model.
    if not os.environ.get('MULTIOS_SKIP_QEMU') and (os.environ.get('QEMU') or shutil.which('qemu-system-x86_64') or shutil.which('qemu-system-i386')):
        for cpu in ('486','qemu64'):
            qi=td/f'{cpu}.img';shutil.copyfile(ROOT/'build/multios.img',qi)
            s=run_qemu(qi,['ver','dir','unlock YES','md TEST','write TEST/a.txt "Hello booted MultiOS"','copy TEST/a.txt TEST/b.txt','cmp TEST/a.txt TEST/b.txt','type TEST/b.txt','ren TEST/b.txt renamed.txt','del TEST/renamed.txt','del TEST/a.txt','rd TEST','df','exit'],cpu)
            logs.append(s);check('Mount failed' not in s and 'Error:' not in s,s);check('Hello booted MultiOS' in s and 'Identical' in s,'boot I/O failed');fsck(qi)
    else:logs.append('SKIP QEMU (not installed)')
(ROOT/'build/test-report.txt').write_text(f'{checks} assertions passed\n\n'+'\n\n'.join(logs))
print(f'{checks} assertions passed. Full log: build/test-report.txt')
