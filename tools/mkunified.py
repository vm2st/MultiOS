#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""NEW ordinary image only. One MBR disk, BIOS kernel + FAT32 ESP + shared selectable data filesystem.
No physical drives, no existing destinations. x64 UEFI keeps Boot Services active.
"""
from pathlib import Path
import argparse, os, shutil, struct, subprocess, tempfile, zipfile, secrets
from mkimage import checksum, P32, P64
ROOT=Path(__file__).resolve().parents[1]
FAT_START=2048
FAT_SECTORS=64*1024*1024//512
DATA_START=FAT_START+FAT_SECTORS

def sources_zip(dest):
    with zipfile.ZipFile(dest,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(ROOT.iterdir()):
            if p.is_file() and (p.suffix in ('.md','.sh','.ps1') or p.name in ('Makefile','LICENSE','THIRD_PARTY_NOTICES.txt')):
                z.write(p,'MultiOS/'+p.name)
        for folder in ('core','drivers','include','lib','platform','ports','profiles','tools','tests','third_party','vendor'):
            for p in sorted((ROOT/folder).rglob('*')):
                if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.pyc':
                    z.write(p,'MultiOS/'+str(p.relative_to(ROOT)))

def build(output,data_fs="exfat"):
    output=Path(output)
    if output.exists() or output.is_symlink(): raise ValueError('Refusing existing output; choose a NEW image filename.')
    boot=(ROOT/'build/boot.bin').read_bytes(); kernel=(ROOT/'build/kernel.bin').read_bytes()
    if len(boot)!=512 or not 1<=len(kernel)<=240*512: raise ValueError('Invalid BIOS boot/kernel size')
    if data_fs not in ('exfat','fat32','ntfs'):raise ValueError('Unsupported data filesystem')
    if data_fs=='exfat':
        source=ROOT/'build/multios.img';offset=2048*512
        if not source.exists():source=ROOT/'build/MultiOS-MultiPulti.img';offset=DATA_START*512
        with source.open('rb') as f:
            f.seek(offset);data=bytearray(f.read())
        if len(data)!=63*1024*1024 or data[3:11]!=b'EXFAT   ':raise ValueError('An exFAT template is required; run make all or use the supplied exFAT default image')
        for base in (0,12*512):
            data[base+64:base+72]=P64(DATA_START)
            data[base+11*512:base+12*512]=P32(checksum(data[base:base+11*512],(106,107,112)))*128
    else:
        data=bytearray(63*1024*1024)
    with tempfile.TemporaryDirectory(prefix='multipulti-') as td:
        td=Path(td);esp=td/'boot.fat'
        if data_fs!='exfat':
            part=td/'data.part'
            with part.open('xb') as f:f.truncate(len(data))
            welcome=td/'WELCOME.TXT';welcome.write_text('MultiOS 0.21-dev-ntfsstreams / MultiPulti | vm2 studios | t.me/v2vproj\n'+data_fs.upper()+' data volume. UNPUBLISHED DEVELOPMENT.\n')
            if data_fs=='fat32':
                subprocess.run(['mkfs.fat','-F','32','-s','1','-h',str(DATA_START),'-n','MULTIOSDATA',str(part)],check=True,stdout=subprocess.DEVNULL)
                subprocess.run(['mcopy','-i',str(part),str(welcome),'::WELCOME.TXT'],check=True)
            else:
                subprocess.run(['mkntfs','-F','-Q','-s','512','-p',str(DATA_START),'-L','MULTIOSDATA',str(part)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                subprocess.run(['ntfscp','-f',str(part),str(welcome),'/WELCOME.TXT'],check=True)
            data=bytearray(part.read_bytes())

        with esp.open('xb') as f:f.truncate(FAT_SECTORS*512)
        subprocess.run(['mkfs.vfat','-F','32','-n','MULTIOSBOOT',str(esp)],check=True,stdout=subprocess.DEVNULL)
        subprocess.run(['mmd','-i',str(esp),'::EFI','::EFI/BOOT','::MULTIOS','::DOCS','::SOURCES','::TOOLS'],check=True)
        sources_zip(td/'PORTING.ZIP')
        (td/'BRAND.TXT').write_text('MultiOS / MultiPulti\nAuthor of the modular porting template: vm2 studios\nCompact author tag: t.me/v2vproj\nTarget family: x86 i386+ and x86-64. Not a guarantee for every CPU/device.\nOriginal project: MIT; upstream notices preserved separately.\n')
        (td/'IMAGE.TXT').write_text('MultiOS 0.21-dev-ntfsstreams (UNPUBLISHED) / MultiPulti | vm2 studios | t.me/v2vproj\nFirst public release is planned only as stable Release 1.0.\nOne editable image: own BIOS i386 kernel + x64 UEFI live app + shared selectable data filesystem.\nUEFI keeps Boot Services; native post-firmware x64 kernel is not implemented.\nBIOS: IDE primary master only. UEFI: firmware boot-disk Block I/O.\nBOOT.CFG selects compiled UEFI modules; BIOS profile is compiled into KERNEL32.BIN.\nC driver source changes require rebuilding and replacing the relevant binary bundle.\nUse external tools/imagectl.py on a powered-off COPY, not a running VM.\nSOURCES/PORTING.ZIP is the source snapshot at image creation.\nKERNEL32.BIN is mirrored into raw boot sectors by imagectl put/install.\nNever update just that file in a generic image editor: BIOS reads the raw mirror.\nWrites are experimental. Back up images. No physical devices tested.\n')
        (td/'START.TXT').write_text((ROOT/'START_HERE_RU.md').read_text(),encoding='utf-8')
        (td/'DRIVERS.TXT').write_text('MultiOS / MultiPulti — vm2 studios | t.me/v2vproj\nStatic C modules, not loadable .drv files.\nSources: SOURCES/PORTING.ZIP, profiles/filesystems.mk, profiles/pc-bios.mk, profiles/uefi.mk.\nRebuild after C changes; install matching BIOS kernel and UEFI binary via imagectl.\nRead DOCS/FS21.MD for current FS profiles and limitations. NTFS RW is UEFI-only and experimental.\n')
        (td/'OSK.ON').write_text('vm2 studios | t.me/v2vproj\n')
        entries={
            'DOCS/FS21.MD':ROOT/'FILESYSTEMS_0_21_RU.md',
            'DOCS/FS18.MD':ROOT/'FILESYSTEMS_0_18_RU.md',
            'DOCS/UPSTREAM.MD':ROOT/'UPSTREAM_FS_0_17_RU.md',
            'DOCS/TXRW.MD':ROOT/'TRANSACTIONAL_RW_RU.md',
            'DOCS/UNICODE.MD':ROOT/'FILESYSTEMS_UNICODE_RU.md',
            'DOCS/FS64.MD':ROOT/'FILESYSTEMS_64_RU.md','DOCS/NTFS2.MD':ROOT/'NTFS_STAGE2_RU.md','DOCS/NTFS1.MD':ROOT/'NTFS_STAGE1_RU.md','START.TXT':td/'START.TXT','DRIVERS.TXT':td/'DRIVERS.TXT',
            'TOOLS/STUDIO.PY':ROOT/'tools/studio.py','DOCS/MODULAR.MD':ROOT/'MODULAR_0_12_RU.md','DOCS/CHANGE12.MD':ROOT/'CHANGES_0_12_RU.md',
            'EFI/BOOT/BOOTX64.EFI':ROOT/'build/BOOTX64.EFI',
            'MULTIOS/KERNEL32.BIN':ROOT/'build/kernel.bin',
            'MULTIOS/BOOT32.BIN':ROOT/'build/boot.bin',
            'MULTIOS/BOOT.CFG':ROOT/'profiles/BOOT-unified.CFG',
            'MULTIOS/BRAND.TXT':td/'BRAND.TXT','MULTIOS/IMAGE.TXT':td/'IMAGE.TXT',
            'MULTIOS/OSK.ON':td/'OSK.ON','MULTIOS/LICENSE.TXT':ROOT/'LICENSE',
            'MULTIOS/THIRDPTY.TXT':ROOT/'THIRD_PARTY_NOTICES.txt',
            'SOURCES/PORTING.ZIP':td/'PORTING.ZIP','TOOLS/IMAGECTL.PY':ROOT/'tools/imagectl.py',
            'DOCS/ONEIMAGE.MD':ROOT/'ONE_IMAGE_RU.md','DOCS/MODULES.MD':ROOT/'MODULES_RU.md',
            'DOCS/CHANGE11.MD':ROOT/'CHANGES_0_11_RU.md','DOCS/FILESYS.MD':ROOT/'FILESYSTEMS_RU.md','DOCS/BUGFIX10.MD':ROOT/'BUGFIX_0_10_RU.md','DOCS/GUARD.MD':ROOT/'KERNEL_STABILITY_RU.md','DOCS/CHANGE09.MD':ROOT/'CHANGES_0_9_RU.md','DOCS/CHANGES.MD':ROOT/'CHANGES_0_8_RU.md','DOCS/USBHID.MD':ROOT/'NATIVE_USB_RU.md','DOCS/CONSOLE.MD':ROOT/'CONSOLE_RU.md',
            'DOCS/BUTTONS.MD':ROOT/'TWO_BUTTONS_RU.md','DOCS/STATUS.MD':ROOT/'RELEASE_POLICY_RU.md',
            'DOCS/KERNEL.MD':ROOT/'MULTIPULTI_RU.md','DOCS/CPU.MD':ROOT/'CPU_COMPAT_RU.md'
        }
        for rel,src in entries.items():
            subprocess.run(['mcopy','-i',str(esp),str(src),'::'+rel],check=True)
        mbr=bytearray(boot)
        mbr[440:444]=struct.pack('<I',secrets.randbits(32) or 1)
        mbr[446:510]=bytes(64)
        mbr[446:462]=struct.pack('<B3sB3sII',0x80,b'\xfe\xff\xff',0xef,b'\xfe\xff\xff',FAT_START,FAT_SECTORS)
        mbr[462:478]=struct.pack('<B3sB3sII',0,b'\xfe\xff\xff',0x0c if data_fs=='fat32' else 7,b'\xfe\xff\xff',DATA_START,len(data)//512)
        fd=os.open(output,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
        with os.fdopen(fd,'wb') as f:
            f.write(mbr);f.write(kernel);f.seek(FAT_START*512)
            with esp.open('rb') as src:shutil.copyfileobj(src,f)
            f.seek(DATA_START*512);f.write(data);f.flush();os.fsync(f.fileno())
    print(f'{output}: {output.stat().st_size//(1024*1024)} MiB; MultiPulti BIOS + x64 UEFI + {data_fs.upper()} data')
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'build/MultiOS-MultiPulti.img')
    parser.add_argument('--data-fs',choices=['exfat','fat32','ntfs'],default='exfat')
    args=parser.parse_args()
    try:build(args.output,args.data_fs)
    except (ValueError,OSError,subprocess.CalledProcessError) as e:parser.exit(1,str(e)+'\n')
