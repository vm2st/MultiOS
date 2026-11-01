#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
"""Inject synchronous write errors at every sector-write boundary of WRITE.
Not a power-loss simulator: it cannot test controller caches or torn sectors.
"""
import pathlib,subprocess,tempfile,shutil,os,struct
ROOT=pathlib.Path(__file__).resolve().parents[1]
binary=ROOT/'build/multios-fault-host'
subprocess.run(['gcc','-std=c11','-O1','-I'+str(ROOT/'lib'),'-I'+str(ROOT/'include'),'-DMULTIOS_FAULT_INJECTION',str(ROOT/'platform/posix/host.c'),*[str(ROOT/'lib'/p) for p in ('util.c','fs.c','exfat.c','fat32.c','ntfs.c','ntfs_meta.c','ntfs_attr.c','block64.c','shell.c')],str(ROOT/'core/hal.c'),str(ROOT/'core/osk.c'),str(ROOT/'core/modules.c'),str(ROOT/'core/cpu.c'),str(ROOT/'core/hardware.c'),str(ROOT/'core/hid.c'),str(ROOT/'core/editor.c'),'-o',str(binary)],check=True)
checks=0;trials=0;report=[]
with tempfile.TemporaryDirectory() as td:
 for cut in range(80):
  img=pathlib.Path(td)/'cut.img';shutil.copyfile(ROOT/'build/multios.img',img)
  env=dict(os.environ,MULTIOS_FAIL_WRITE_AT=str(cut))
  p=subprocess.run([str(binary),str(img),'--allow-write'],input='unlock YES\nwrite probe.txt "fault test"\nexit\n',text=True,capture_output=True,env=env,timeout=20)
  report.append(f'Cut {cut}: return={p.returncode}\n{p.stdout}')
  with img.open('rb') as f:f.seek(2048*512+106);flags=struct.unpack('<H',f.read(2))[0]
  checks+=1
  if p.returncode==0:
   assert not flags&2 and 'Error:' not in p.stdout,p.stdout
   break
  assert p.returncode==1 and 'Error:' in p.stdout,p.stdout
  checks+=1
  assert (flags&2 if cut else not flags&2),f'Dirty state lost at write {cut}'
  # Restart must not silently clear a dirty bit or allow subsequent writes.
  if cut:
   again=subprocess.run([str(binary),str(img),'--allow-write'],input='unlock YES\nexit\n',text=True,capture_output=True,timeout=20)
   checks+=1;assert 'Error:' in again.stdout and 'dirty volume' in again.stdout,again.stdout
  trials+=1
 else:raise AssertionError('operation took more writes than test limit')
(ROOT/'build/fault-report.txt').write_text(f'{checks} assertions passed across {trials} failing write positions; then one success.\n\n'+'\n'.join(report))
print(f'{checks} assertions passed across {trials} injected write failures; dirty-state remount verified.')
