#!/usr/bin/env python3
"""MultiOS filesystem regression gate. No physical disks; failure never becomes PASS."""
from pathlib import Path
import argparse,json,os,shutil,subprocess,sys,time
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--quick',action='store_true',help='Skip long fault matrices; NOT the full gate')
p.add_argument('--output',type=Path,default=root/'build/fs-check')
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
report={'mode':'quick' if a.quick else 'full','status':'running','hardware_tested':False,'release_certification':False,'steps':[]}
def save(): (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
def step(name,cmd,extra=None,timeout=3600):
 env=os.environ.copy();env.update(extra or {});start=time.monotonic();log=out/(name+'.log')
 print('RUN '+name,flush=True)
 try:
  with log.open('wb') as f:r=subprocess.run(cmd,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=timeout).returncode
 except subprocess.TimeoutExpired:r=124
 except OSError as e:log.write_text(str(e)+'\n');r=127
 report['steps'].append({'name':name,'returncode':r,'seconds':round(time.monotonic()-start,3),'log':log.name,'status':'pass' if r==0 else 'fail'});save()
 if r:raise RuntimeError(name+' failed; see '+str(log))
 print('PASS '+name,flush=True)
try:
 missing=[x for x in ('make','cc','python3','mkfs.fat','fsck.fat','fsck.exfat','mkntfs','ntfsfix','ntfscat','mtype') if not shutil.which(x)]
 if missing:raise RuntimeError('Missing dependencies: '+', '.join(missing))
 step('core',['make','-j4','test-block64','test-tx-abort','test-undo','test-write-guard','test-repeated-crash','test-hal','test-fs-driver-contract','test-file64-epoch'])
 targets=['build/fs-path-probe','build/fs-path-probe-sanitize','build/fs-tx-contract-probe','build/fs-tx-contract-probe-sanitize','build/fs-hal-reject-probe','build/fs-hal-reject-probe-sanitize','build/fs-media-failure-probe','build/fs-media-failure-probe-sanitize','build/fs-media-rw-probe','build/fs-media-rw-probe-sanitize','build/fs-random-probe','build/fs-copy-probe','build/ntfs-stream-probe']
 step('build',['make','-j4','FS_PROFILE=profiles/filesystems-ntfs3g.mk']+targets)
 step('paths',['python3','tests/path_safety.py'])
 step('paths-sanitize',['python3','tests/path_safety.py'],{'MULTIOS_PATH_PROBE':str(root/'build/fs-path-probe-sanitize')})
 for suffix in ('','-sanitize'):
  step('tx-contract'+suffix,['python3','tests/path_safety.py'],{'MULTIOS_PATH_PROBE':str(root/('build/fs-tx-contract-probe'+suffix)),'MULTIOS_PATH_JOURNAL_RECORDS':'4096'})
 for suffix in ('','-sanitize'):
  step('hal-reject'+suffix,['python3','tests/path_safety.py'],{'MULTIOS_PATH_PROBE':str(root/('build/fs-hal-reject-probe'+suffix))})
 for suffix in ('','-sanitize'):
  step('media-failure'+suffix,['python3','tests/path_safety.py'],{'MULTIOS_PATH_PROBE':str(root/('build/fs-media-failure-probe'+suffix))})
 step('periphery',['make','-j4','test-usb-enum','test-usb-bot','test-usb-bulk-hosts','test-fatfs-bridge','test-unlock-contract'])
 if not a.quick:
  step('media-rw',['python3','tests/media_rw.py'])
  step('media-rw-sanitize',['python3','tests/media_rw.py'],{'MULTIOS_MEDIA_RW_PROBE':str(root/'build/fs-media-rw-probe-sanitize')})
  for name,script in [('random-io','random_io.py'),('staged-copy','staged_copy.py'),('ntfs-streams','ntfs_streams.py')]:step(name,['python3','tests/'+script])
 step('bios-build',['make','bios-rw'])
 step('uefi-build',['make','-j4','FS_PROFILE=profiles/filesystems-ntfs3g.mk','uefi'])
 report['status']='pass';save();print('PASS '+report['mode']+' software gate. Not hardware certification; boot tests and release acceptance remain separate.')
except Exception as e:
 report['status']='fail';report['error']=str(e);save();print('FAIL '+str(e),file=sys.stderr);sys.exit(1)
