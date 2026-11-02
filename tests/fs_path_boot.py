#!/usr/bin/env python3
"""Boot rebuilt images; verify invalid-name refusal and normal RW persistence."""
from pathlib import Path
import os,sys,tempfile,subprocess
from bios_rw_boot import boot as bios_boot, extract
from uefi_smoke import boot as uefi_boot
root=Path(__file__).resolve().parents[1]
images=Path(sys.argv[1]).resolve()
commands=['ver','fs','unlock YES','write KEEP.TXT "KEEP"','write KEEP.TXT. "LOST"','append "KEEP.TXT " "LOST"','ren KEEP.TXT BAD.','del KEEP.TXT.','md BAD.','type KEEP.TXT','copy KEEP.TXT COPY.TXT','resize COPY.TXT 8193','writeat COPY.TXT 4095 "PATCH"','resize COPY.TXT 5','cmp KEEP.TXT COPY.TXT','lock']
def run(*args):
 p=subprocess.run(list(map(str,args)),capture_output=True,timeout=120);assert not p.returncode,(args,p.stderr[-1000:]);return p.stdout
for kind in ('fat32','exfat','ntfs'):
 for mode in os.environ.get('MULTIOS_BOOT_MODES','bios,uefi').split(','):
  with tempfile.TemporaryDirectory(prefix='path-boot-',dir=root/'build') as td:
   work=Path(td);source=images/f'MultiOS-0.37-usb-{kind}.img'
   if mode=='bios':text,elapsed,image=bios_boot(source,work,commands+['exit'])
   else:
    cfg=(root/'profiles/BOOT-unified.CFG').read_text().replace('osk=auto','osk=off').replace('output=uefi-text','output=uefi-plain')
    text,elapsed,image=uefi_boot(commands+['poweroff'],work,source=source,config=cfg,osk=False)
   assert '0.37-dev-mediaguard' in text and text.count('Error:')==5 and 'Identical' in text,text[-7000:]
   part=work/'data.part';extract(image,part)
   run({'fat32':'fsck.fat','exfat':'fsck.exfat','ntfs':'ntfsfix'}[kind],'-n',part)
   for name in ('KEEP.TXT','COPY.TXT'):
    if kind=='fat32':b=run('mtype','-i',part,'::'+name)
    elif kind=='ntfs':b=run('ntfscat',part,'/'+name)
    else:b=run(root/'build/fs-read-probe',image,'/'+name,'0','5')
    assert b==b'KEEP\n',(kind,mode,name,b)
   print(f'PASS {mode}/{kind}: 0.37 boot, 5 invalid-name commands rejected, write/copy/resize/write-at, comparison and persisted bytes; basic external checker. Emulator only.',flush=True)
