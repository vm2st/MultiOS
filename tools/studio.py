#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti, unpublished development.
"""Beginner workspace for powered-off regular FAT32-data images (Linux/WSL).
Unpack -> edit ordinary files -> plan -> apply --backup NEWFILE --yes.
Never targets devices. DATA is rebuilt in a private snapshot, not in place.
"""
import argparse, hashlib, json, os, shutil, stat, subprocess, tempfile
from pathlib import Path, PurePosixPath
try:
    import imagectl as ctl
except ModuleNotFoundError:
    # FAT image ships uppercase names; source tree uses conventional lowercase.
    import importlib.util
    spec=importlib.util.spec_from_file_location('imagectl',Path(__file__).with_name('IMAGECTL.PY'))
    ctl=importlib.util.module_from_spec(spec);spec.loader.exec_module(ctl)
MAX_FILES=4096; MAX_TOTAL=48*1024*1024
SYSTEM={'kernel32.bin':'MULTIOS/KERNEL32.BIN','boot32.bin':'MULTIOS/BOOT32.BIN','bootx64.efi':'EFI/BOOT/BOOTX64.EFI'}

def run(*args):
    p=subprocess.run(list(map(str,args)),capture_output=True,timeout=120)
    if p.returncode:raise ValueError('Command failed: '+str(args[0])+'\n'+(p.stdout+p.stderr).decode(errors='replace')[:3000])
    return p.stdout

def sha(data):return hashlib.sha256(data).hexdigest()
def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def safe_name(name):
    # Workspace remains deliberately ASCII-only: external mtools Unicode round-trip is not guaranteed.
    p=PurePosixPath(name)
    if not name or name.startswith('/') or str(p)!=name or len(p.parts)>16 or len(name)>240:raise ValueError('Unsafe/deep path: '+name)
    for part in p.parts:
        if part in ('.','..') or len(part)>200 or part[-1:] in (' ','.') or any(ord(c)<32 or ord(c)>126 or c in '\\:*?"<>|[]' for c in part):raise ValueError('Unsupported filename: '+name)
    return name

def regular(path,limit):
    s=path.lstat()
    if not stat.S_ISREG(s.st_mode) or s.st_nlink!=1:raise ValueError('Not a single-link regular file: '+str(path))
    return ctl.source_bytes(path,limit)

def tree(folder):
    if not stat.S_ISDIR(folder.lstat().st_mode):raise ValueError('Expected real directory: '+str(folder))
    dirs=[];files={};total=0;fold=set()
    def visit(d):
        nonlocal total
        for p in sorted(d.iterdir()):
            name=safe_name(p.relative_to(folder).as_posix());key=name.lower()
            if key in fold:raise ValueError('Case-insensitive filename collision: '+name)
            fold.add(key)
            if len(fold)>MAX_FILES:raise ValueError('Too many workspace entries')
            mode=p.lstat().st_mode
            if stat.S_ISDIR(mode):dirs.append(name);visit(p)
            elif stat.S_ISREG(mode):
                b=regular(p,MAX_TOTAL);total+=len(b)
                if total>MAX_TOTAL:raise ValueError('Workspace exceeds 48 MiB')
                files[name]=b
            else:raise ValueError('Symlinks/devices/FIFOs are not accepted: '+name)
    visit(folder);return dirs,files

def snapshot(image,dest):
    with Path(image).open('rb') as f,Path(dest).open('wb') as out:
        f.seek(ctl.DATA_START*512);shutil.copyfileobj(f,out)
    b=Path(dest).open('rb')
    try:boot=b.read(512)
    finally:b.close()
    if boot[82:90]!=b'FAT32   ':raise ValueError('Studio requires FAT32 DATA. NTFS/exFAT are not rewritten by this tool.')
    run('fsck.fat','-n',dest)

def unpack(image,dest):
    dest=Path(os.path.abspath(dest))
    if dest.exists() or dest.is_symlink():raise ValueError('Choose a NEW workspace folder')
    with ctl.opened(image) as (f,_),tempfile.TemporaryDirectory(prefix='.multios-unpack-',dir=dest.parent) as td:
        stage=Path(td)/'workspace';stage.mkdir();(stage/'files').mkdir();(stage/'system').mkdir()
        part=Path(td)/'data.fat';snapshot(image,part)
        listing=run('mdir','-b','-s','-i',part,'::/').decode('ascii').splitlines()
        if len(listing)>MAX_FILES:raise ValueError('Too many FAT entries')
        paths=[];seen=set()
        for line in listing:
            if not line.startswith('::/'):raise ValueError('Unexpected FAT directory listing')
            isdir=line.endswith('/');name=safe_name(line[3:].rstrip('/'))
            if name.lower() in seen:raise ValueError('Duplicate FAT name')
            seen.add(name.lower());paths.append((name,isdir))
        for name,isdir in sorted(paths,key=lambda row:row[0].count('/')):
            target=stage/'files'/name;target.parent.mkdir(parents=True,exist_ok=True)
            if isdir:target.mkdir(exist_ok=True)
            else:run('mcopy','-n','-i',part,'::/'+name,target)
        dirs,files=tree(stage/'files')
        ctl.extract(image,'MULTIOS/BOOT.CFG',stage/'settings.ini')
        for name,rel in SYSTEM.items():ctl.extract(image,rel,stage/'system'/name)
        manifest={'version':1,'image_sha256':digest(image),'dirs':dirs,'files':{n:sha(b) for n,b in files.items()},'system':{n:sha((stage/'system'/n).read_bytes()) for n in SYSTEM},'settings':sha((stage/'settings.ini').read_bytes())}
        (stage/'.multios.json').write_text(json.dumps(manifest,indent=2)+'\n')
        (stage/'START-HERE.txt').write_text('MultiOS / MultiPulti — vm2 studios | t.me/v2vproj\nНЕОПУБЛИКОВАННАЯ РАЗРАБОТКА 0.12-dev\n\nfiles/ — ваши файлы на FAT32 DATA. Можно добавлять/удалять папки и ASCII-файлы.\nsettings.ini — настройки UEFI; osk=on/off/auto. На профиль BIOS не влияет.\nsystem/ — готовые бинарные файлы; исходники C сюда не копируют.\nДрайверы меняют в исходниках, собирают make, затем заменяют согласованный набор бинарников.\n.multios.json — служебная опись. Не редактировать.\n\nplan показывает изменения; apply --backup НОВОЕ_ИМЯ.img --yes сохраняет их.\nЗакройте виртуальную машину. Работайте только с копией образа!\napply пересоздаёт только FAT32 DATA из files/: даты/атрибуты/имена 8.3 могут измениться.\nИсходное содержимое остаётся в обязательной полной резервной копии.\nПосле apply для следующего редактирования заново выполните unpack.\n',encoding='utf-8')
        os.rename(stage,dest)
    print('Workspace created: '+str(dest))

def load(work):
    work=Path(work)
    if not stat.S_ISDIR(work.lstat().st_mode):raise ValueError('Workspace must be a real directory')
    m=json.loads(regular(work/'.multios.json',2*1024*1024))
    if m.get('version')!=1:raise ValueError('Unknown workspace version')
    dirs,files=tree(work/'files');cfg=regular(work/'settings.ini',2048).decode('ascii')
    # Same bounded UEFI grammar; no arbitrary plugin filename is executed.
    keys=set();valid={'output','input','block','power','osk','output.options','input.options','block.options','power.options'}
    for line in cfg.splitlines():
        line=line.strip()
        if not line or line.startswith(('#',';')):continue
        if '=' not in line:raise ValueError('Expected key=value in settings.ini')
        key,value=map(str.strip,line.split('=',1))
        if key not in valid or key in keys:raise ValueError('Unknown/duplicate setting: '+key)
        ctl.config_text('',key,value);keys.add(key)
    if not {'output','input','block','power'}.issubset(keys):raise ValueError('Missing required module IDs')
    sysdir=work/'system'
    if not stat.S_ISDIR(sysdir.lstat().st_mode) or set(p.name for p in sysdir.iterdir())!=set(SYSTEM):raise ValueError('system/ must contain exactly the three supplied binaries')
    for n in SYSTEM:regular(sysdir/n,16*1024*1024)
    binaries={'kernel32.bin':ctl.kernel_bytes(sysdir/'kernel32.bin'),'boot32.bin':ctl.boot_bytes(sysdir/'boot32.bin'),'bootx64.efi':ctl.efi_bytes(sysdir/'bootx64.efi')}
    if binaries['boot32.bin'][432:440]!=b'MP11\xf0\x00\x01\x00':raise ValueError('Incompatible boot bundle')
    changes=[]
    old=m['files']
    for n in sorted(set(old)|set(files)):
        if n not in files:changes.append('DELETE files/'+n)
        elif n not in old:changes.append('ADD files/'+n)
        elif sha(files[n])!=old[n]:changes.append('UPDATE files/'+n)
    for n in sorted(set(dirs)^set(m['dirs'])):changes.append(('ADD' if n in dirs else 'DELETE')+' directory '+n)
    if sha(cfg.encode('ascii'))!=m['settings']:changes.append('UPDATE settings.ini (UEFI only)')
    for n,b in binaries.items():
        if sha(b)!=m['system'][n]:changes.append('UPDATE system/'+n)
    return m,dirs,files,cfg.encode('ascii'),binaries,changes

def apply(image,work,backup,yes):
    m,dirs,files,cfg,binaries,changes=load(work)
    for line in changes:print(line)
    if not changes:print('No changes.');return
    if not yes:raise ValueError('Applying requires --yes and --backup NEWFILE')
    backup=Path(os.path.abspath(backup))
    if backup.exists() or backup.is_symlink():raise ValueError('Backup must not exist')
    def edit(temp):
        if digest(temp)!=m['image_sha256']:raise ValueError('Image changed since unpack. Export a new workspace; no write performed.')
        with tempfile.TemporaryDirectory(prefix='multios-studio-') as td:
            td=Path(td);part=td/'data.fat';snapshot(temp,part);size=part.stat().st_size
            # Backup comes from the locked, verified transaction copy BEFORE mutation.
            with backup.open('xb') as out,temp.open('rb') as src:
                shutil.copyfileobj(src,out);out.flush();os.fsync(out.fileno())
            dfd=os.open(backup.parent,os.O_RDONLY|os.O_DIRECTORY)
            try:os.fsync(dfd)
            finally:os.close(dfd)
            part.unlink()
            with part.open('xb') as out:out.truncate(size)
            run('mkfs.fat','-F','32','-s','1','-h',ctl.DATA_START,'-n','MULTIOSDATA',part)
            for n in sorted(dirs,key=lambda n:n.count('/')):run('mmd','-i',part,'::/'+n)
            for i,(n,b) in enumerate(sorted(files.items())):
                payload=td/('payload-'+str(i));payload.write_bytes(b);run('mcopy','-i',part,payload,'::/'+n);payload.unlink()
            if part.stat().st_size!=size:raise ValueError('DATA size changed')
            run('fsck.fat','-n',part)
            with temp.open('r+b') as out,part.open('rb') as src:out.seek(ctl.DATA_START*512);shutil.copyfileobj(src,out)
            ctl.inject_data(temp,'MULTIOS/BOOT.CFG',cfg)
            # Use the already validated byte snapshots, never reread edited sources.
            for n,b in binaries.items():
                if sha(b)==m['system'][n]:continue
                ctl.inject_data(temp,SYSTEM[n],b)
                if n=='kernel32.bin':
                    with temp.open('r+b') as out:out.seek(512);out.write(b+bytes(ctl.KERNEL_BYTES-len(b)))
                elif n=='boot32.bin':
                    with temp.open('r+b') as out:out.write(b[:440])
    ctl.transaction(image,edit)
    print('Backup: '+str(backup)+'\nUnpack again before the next edit. '+ctl.CREDIT)

def main():
    p=argparse.ArgumentParser(description=__doc__);s=p.add_subparsers(dest='command',required=True)
    q=s.add_parser('unpack');q.add_argument('image',type=Path);q.add_argument('workspace',type=Path)
    q=s.add_parser('plan');q.add_argument('workspace',type=Path)
    q=s.add_parser('apply');q.add_argument('image',type=Path);q.add_argument('workspace',type=Path);q.add_argument('--backup',type=Path,required=True);q.add_argument('--yes',action='store_true')
    a=p.parse_args()
    if a.command=='unpack':unpack(a.image,a.workspace)
    elif a.command=='plan':
        changes=load(a.workspace)[-1];print('\n'.join(changes) if changes else 'No changes.')
    else:apply(a.image,a.workspace,a.backup,a.yes)
if __name__=='__main__':
    try:main()
    except (OSError,ValueError,KeyError,TypeError,UnicodeError,subprocess.SubprocessError) as e:raise SystemExit('studio: '+str(e))
