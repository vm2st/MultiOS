#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — standalone UEFI COPYPAUSE/COPYRESUME, not BIOS NTFS RW.
from pathlib import Path
import tempfile,subprocess,sys,zlib,struct
from uefi_smoke import boot
r=Path(__file__).resolve().parents[1]
for kind in ('fat32','exfat','ntfs'):
 w=r/'build'/('020-boot-'+kind);w.mkdir(exist_ok=True)
 with tempfile.TemporaryDirectory(prefix='copy-boot-',dir=r/'build') as td:
  t=Path(td);source=t/'seed.img';source.write_bytes((r/'build'/f'MultiOS-0.21-dev-ntfsstreams-{kind}.img').read_bytes())
  bulk=t/'bulk.bin';payload=bytes(range(256))*(4*1024*1024//256)+bytes(range(123));bulk.write_bytes(payload)
  p=subprocess.run([str(r/'build/fs-copy-probe'),str(source),'seed',str(bulk)],capture_output=True);assert not p.returncode,(p.stdout,p.stderr)
  cfg=(r/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
  commands=['ver','fs','unlock YES','copypause SMALL.BIN RESULT.BIN 16384','copyresume SMALL.BIN RESULT.BIN MC000001.TMP','cmp SMALL.BIN RESULT.BIN','crc32 RESULT.BIN','copyatomic SMALL.BIN ATOMIC.BIN','cmp SMALL.BIN ATOMIC.BIN','lock','poweroff']
  text,seconds,image=boot(commands,w,config=cfg,osk=False,source=source,timeout=180)
  (w/'console.log').write_text(text)
  assert text.count('Error:')==1 and 'copy cancelled' in text,text[-6000:]
  assert 'Staging retained' in text and text.count('Identical')==2 and 'Copied and verified 32769 bytes.' in text,text[-6000:]
  expected=payload[:32769];crc=f'{zlib.crc32(expected):08X}';assert crc in text,text[-6000:]
  with image.open('rb') as f:
   m=f.read(512);start,n=struct.unpack_from('<II',m,470);f.seek(start*512);part=w/'data.part';part.write_bytes(f.read(n*512))
  p=subprocess.run(['fsck.fat' if kind=='fat32' else 'fsck.exfat' if kind=='exfat' else 'ntfsfix','-n',str(part)],capture_output=True);assert not p.returncode,(p.stdout,p.stderr)
  if kind=='fat32':actual=subprocess.check_output(['mtype','-i',str(part),'::RESULT.BIN'])
  elif kind=='ntfs':actual=subprocess.check_output(['ntfscat',str(part),'/RESULT.BIN'])
  else:actual=subprocess.check_output([str(r/'build/fs-read-probe'),str(image),'/RESULT.BIN','0','32769'],stderr=subprocess.DEVNULL)
  assert actual==expected
  print(f'PASS {kind} UEFI: COPYPAUSE, COPYRESUME, CMP, CRC32={crc}, COPYATOMIC; external checker and readback after poweroff. Prompt {seconds:.3f}s (emulator only).',flush=True)
print('PASS all 3 final images: standalone UEFI, firmware USB input/storage, Boot Services retained; not full x64 native kernel.')
