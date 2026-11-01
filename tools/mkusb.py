#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Create NEW release files and an ordinary image file, NEVER a physical device.
UEFI x64 FAT32 USB layout. The exFAT volume is a DATA.IMG file, not host FS.
"""
from pathlib import Path
import struct,subprocess,tempfile,shutil,os,argparse
from mkimage import checksum,P32
ROOT=Path(__file__).resolve().parents[1]

def build(config):
 files=ROOT/'build/USB_FILES';files.mkdir(exist_ok=True)
 (files/'EFI/BOOT').mkdir(parents=True,exist_ok=True);(files/'MULTIOS').mkdir(exist_ok=True)
 shutil.copyfile(ROOT/'build/BOOTX64.EFI',files/'EFI/BOOT/BOOTX64.EFI')
 with (ROOT/'build/multios.img').open('rb') as f:f.seek(2048*512);data=bytearray(f.read())
 for base in (0,12*512):
  data[base+64:base+72]=bytes(8) # superfloppy: partition offset is now zero
  region=data[base:base+11*512]
  data[base+11*512:base+12*512]=P32(checksum(region,(106,107,112)))*128
 (files/'MULTIOS/DATA.IMG').write_bytes(data)
 (files/'MULTIOS/OSK.ON').write_text('Remove this file to start with the on-screen keyboard disabled.\n')
 shutil.copyfile(config,files/'MULTIOS/BOOT.CFG')
 shutil.copyfile(ROOT/'THIRD_PARTY_NOTICES.txt',files/'MULTIOS/LICENSES.TXT')
 (files/'MULTIOS/README.TXT').write_text('MultiOS 0.6 / MultiPulti | vm2 studios | t.me/v2vproj: x64 UEFI only; firmware services stay active.\nF2 or Ctrl+O toggles OSK. Arrows select, Enter presses selected key.\nOSK ON/OFF/TOGGLE; POWEROFF; REBOOT; DRIVERS; MODULES.\nMULTIOS/BOOT.CFG selects compiled drivers; see MODULES_RU.md in source archive.\nDATA.IMG is experimental exFAT. Keep backups; write disabled until UNLOCK YES.\n')
 dest=ROOT/'build/multios-usb-uefi.img'
 if dest.exists():dest.unlink() # predetermined build output, not a user device
 with tempfile.TemporaryDirectory(prefix='multios-usb-') as td:
  esp=Path(td)/'esp.fat'
  with esp.open('wb') as f:f.truncate(128*1024*1024)
  subprocess.run(['mkfs.vfat','-F','32','-n','MULTIOS',str(esp)],check=True,stdout=subprocess.DEVNULL)
  subprocess.run(['mmd','-i',str(esp),'::EFI','::EFI/BOOT','::MULTIOS'],check=True)
  for rel in ('EFI/BOOT/BOOTX64.EFI','MULTIOS/DATA.IMG','MULTIOS/OSK.ON','MULTIOS/BOOT.CFG','MULTIOS/README.TXT','MULTIOS/LICENSES.TXT'):
   subprocess.run(['mcopy','-i',str(esp),str(files/rel),'::'+rel],check=True)
  mbr=bytearray((ROOT/'build/usb-mbr.bin').read_bytes());start=2048;count=128*1024*1024//512
  mbr[446:462]=struct.pack('<B3sB3sII',0,b'\xfe\xff\xff',0xef,b'\xfe\xff\xff',start,count)
  with dest.open('xb') as f:
   f.write(mbr);f.seek(start*512)
   with esp.open('rb') as i:shutil.copyfileobj(i,f)
   f.flush();os.fsync(f.fileno())
 print(f'Created {dest.name}: 129 MiB, MBR + FAT32, UEFI x64, EFI/BOOT/BOOTX64.EFI')
 print('USB_FILES may instead be copied to an existing FAT32 USB without formatting.')
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__)
 parser.add_argument('--config',type=Path,default=ROOT/'profiles/BOOT.CFG')
 build(parser.parse_args().config)
