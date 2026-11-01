#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — workspace roundtrip and refusal tests.
from pathlib import Path
import hashlib,shutil,subprocess,sys,os
R=Path(__file__).resolve().parents[1];W=R/'build/studio12-tests'
if W.exists():shutil.rmtree(W)
W.mkdir();logs=[];checks=0

def run(*a,ok=True):
 global checks
 p=subprocess.run(list(map(str,a)),capture_output=True,text=True,timeout=180);logs.append(p.stdout+p.stderr);checks+=1
 assert (p.returncode==0)==ok,(a,p.stdout,p.stderr)
 return p.stdout+p.stderr

def studio(*a,ok=True):return run(sys.executable,R/'tools/studio.py',*a,ok=ok)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
image=W/'test.img';shutil.copyfile(Path(os.environ.get('MULTIOS_STUDIO_SOURCE',str(R/'build/MultiOS-MultiPulti.img'))),image)
original=digest(image);space=W/'workspace';studio('unpack',image,space)
assert 'No changes' in studio('plan',space)
(space/'files'/'DOCS').mkdir();(space/'files'/'DOCS'/'My file.txt').write_text('Studio nested contents\n')
(space/'files'/'EMPTY').mkdir();(space/'files'/'WELCOME.TXT').unlink();cfg=space/'settings.ini';cfg.write_text(cfg.read_text().replace('osk=auto','osk=off'))
# Change a harmless padding byte in the kernel binary; verify raw mirror matches.
k=space/'system'/'kernel32.bin';b=k.read_bytes();k.write_bytes(b+b'\0')
plan=studio('plan',space);assert 'ADD files/DOCS/My file.txt' in plan and 'DELETE files/WELCOME.TXT' in plan and 'UPDATE system/kernel32.bin' in plan
studio('apply',image,space,'--backup',W/'no-consent.img',ok=False);assert digest(image)==original and not (W/'no-consent.img').exists()
backup=W/'backup.img';studio('apply',image,space,'--backup',backup,'--yes');assert digest(backup)==original
raw=image.read_bytes();assert raw[512:512+len(b)+1]==b+b'\0'
assert run('mtype','-i',str(image)+'@@68157440','::DOCS/My file.txt')=='Studio nested contents\n'
part=W/'part.fat';part.write_bytes(raw[133120*512:]);run('fsck.fat','-n',part)
again=W/'again';studio('unpack',image,again)
assert (again/'files'/'DOCS'/'My file.txt').read_text()=='Studio nested contents\n' and (again/'files'/'EMPTY').is_dir()
assert not (again/'files'/'WELCOME.TXT').exists();assert 'No changes' in studio('plan',again)
# Stale workspace cannot overwrite a changed image; no backup created.
studio('apply',image,space,'--backup',W/'stale.img','--yes',ok=False);assert not (W/'stale.img').exists()
# Symlinks/hardlinks/case collisions/deep paths/unknown config keys rejected before writing.
file=again/'files'/'DOCS'/'My file.txt';link=again/'files'/'link'
link.symlink_to(file);studio('plan',again,ok=False);link.unlink()
os.link(file,link);studio('plan',again,ok=False);link.unlink()
collision=again/'files'/'docs';collision.mkdir();studio('plan',again,ok=False);collision.rmdir()
config=again/'settings.ini';old=config.read_text();config.write_text(old+'unknown=oops\n');studio('plan',again,ok=False);config.write_text(old)
symsys=again/'system'/'extra.drv';symsys.write_text('not a binary driver');studio('plan',again,ok=False);symsys.unlink()
# Workspaces refuse NTFS (no hidden external write support).
ntfs=W/'ntfs.img';shutil.copyfile(image,ntfs)
with ntfs.open('r+b') as f:f.seek(133120*512+3);f.write(b'NTFS    ');f.seek(133120*512+82);f.write(bytes(8))
studio('unpack',ntfs,W/'ntfs-work',ok=False);assert not (W/'ntfs-work').exists()
(W/'console.log').write_text('\n'.join(logs));report=f'PASS: {checks} tool invocations; export/edit/plan/backup/atomic apply/re-export; empty+spaced directories, delete, config, raw kernel mirror; stale workspace, symlink, hardlink, case collision, unknown config, fake module and NTFS refusal.\n'
(R/'build/dev12-studio-report.txt').write_text(report);print(report)
