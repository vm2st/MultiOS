#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — isolated extended i386 kernel build, GPL combined.
from pathlib import Path
import subprocess,os,sys
r=Path(__file__).resolve().parents[1];os.chdir(r);fault='--fault-test' in sys.argv[1:];b=r/('build/bios-rw-fault' if fault else 'build/bios-rw');b.mkdir(parents=True,exist_ok=True)
cc=os.environ.get('CC','gcc');ld=os.environ.get('LD','ld')
def run(*a):subprocess.run(list(map(str,a)),check=True)
base=['-DMULTIOS_USB_HOST_LIMIT=4','-m32','-march=i386','-Wa,-march=i386','-mtune=i386','-Os','-Werror=implicit-function-declaration','-Werror=int-conversion','-ffreestanding','-fno-builtin','-fno-pie','-fno-pic','-mno-sse','-mno-mmx','-msoft-float','-fcf-protection=none','-fno-asynchronous-unwind-tables','-ffunction-sections','-fdata-sections','-fstack-protector-strong','-mstack-protector-guard=global','-Ilib','-Iinclude','-I.']
extra=['-D_FILE_OFFSET_BITS=64','-D_TIME_BITS=64','-DMULTIOS_STORAGE_TX','-DMULTIOS_FATFS','-DMULTIOS_NTFS3G','-DHAVE_CONFIG_H','-D_GNU_SOURCE','-DNO_NTFS_DEVICE_DEFAULT_IO_OPS','-DMPNTFS_BIOS','-Iports/ntfs3g/bios','-Iports/ntfs3g','-Ivendor/ntfs-3g/include/ntfs-3g','-Ivendor/fatfs/port']
names='acls attrib attrlist bitmap bootsect cache collate compress debug device dir ea efs index inode ioctl lcnalloc logfile mft misc mst object_id realpath reparse runlist security unistr xattrs volume_port log'.split()
libs='usb_bot fs fs_copy exfat fat32 ntfs ntfs_meta ntfs_attr block64 undo storage_tx fatfs_core fatfs_unicode fatfs_adapter div64 ntfs3g_adapter util shell'.split()
files=[f'lib/{n}.c' for n in libs]+[f'core/{n}.c' for n in 'hal osk modules boot_profile cpu hardware hid editor'.split()]+[f'drivers/pc-bios/{n}.c' for n in 'text keys pci usb uhci ohci ehci usb-storage ata power'.split()]+['platform/x86/cpu.c','platform/pc-bios/platform.c','platform/pc-bios/exceptions.c','platform/pc-bios/start.S','platform/pc-bios/exceptions.S']
if fault:extra+=['-DMULTIOS_BIOS_FAULT_TEST']
objs=[]
for p in files+[f'vendor/ntfs-3g/libntfs-3g/{n}.c' if n not in ('volume_port','log') else f'ports/ntfs3g/{n}.c' for n in names]+['ports/ntfs3g/bios/allocator.c','ports/ntfs3g/bios/runtime.c','ports/ntfs3g/bios/divsigned.c']:
 dest=b/(p.replace('/','_')+'.o');opts=base+extra+['-std=gnu11']
 if 'vendor/ntfs-3g' in p or p in ('ports/ntfs3g/volume_port.c','ports/ntfs3g/log.c','ports/ntfs3g/bios/runtime.c'):opts+=['-include','ports/ntfs3g/rename.h']
 run(cc,*opts,'-c',p,'-o',dest);objs.append(dest)
run(ld,'-m','elf_i386','--gc-sections','-T','platform/pc-bios/rw/kernel.ld',*objs,'-o',b/'kernel.elf')
run('objcopy','-O','binary',b/'kernel.elf',b/'kernel.bin');assert (b/'kernel.bin').stat().st_size<=(2048-18)*512
for src in ['mbr.S','loader.S','loader.c']:run(cc,*base,*(['-DMULTIOS_BIOS_FAULT_TEST'] if fault else []),'-fno-stack-protector','-c','platform/pc-bios/rw/'+src,'-o',b/(src+'.o'))
run(ld,'-m','elf_i386','-T','platform/pc-bios/boot.ld',b/'mbr.S.o','-o',b/'mbr.bin')
run(ld,'-m','elf_i386','--gc-sections','-T','platform/pc-bios/rw/loader.ld',b/'loader.S.o',b/'loader.c.o','-o',b/'loader.elf')
run('objcopy','-O','binary',b/'loader.elf',b/'loader.bin');assert (b/'loader.bin').stat().st_size<=15*512
assert not subprocess.check_output(['nm','-u',b/'kernel.elf']).strip()
print('PASS freestanding i386 NTFS RW build:',(b/'kernel.bin').stat().st_size,'kernel bytes; bounded E820 RAM/CRC32 loader; NOT stable release.')
