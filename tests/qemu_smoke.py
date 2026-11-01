#!/usr/bin/env python3
# vm2 studios | t.me/v2vproj — MultiOS / MultiPulti
import subprocess,time,select,os,pathlib,shutil,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run_qemu(image,commands,cpu='qemu64',timeout=30):
    qemu=os.environ.get('QEMU') or shutil.which('qemu-system-x86_64') or shutil.which('qemu-system-i386')
    if not qemu:raise RuntimeError('QEMU missing')
    args=[qemu,'-machine','pc','-accel','tcg','-cpu',cpu,'-m','32M','-drive',f'file={image},format=raw,if=ide,index=0','-display','none','-monitor','none','-serial','stdio','-no-reboot','-device','isa-debug-exit,iobase=0xf4,iosize=0x04']
    p=subprocess.Popen(args,stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    result=bytearray();pos=0
    try:
        for cmd in commands:
            deadline=time.monotonic()+timeout
            while b'> ' not in result[pos:]:
                if time.monotonic()>deadline:raise TimeoutError(bytes(result).decode(errors='replace'))
                ready,_,_=select.select([p.stdout],[],[],.1)
                if ready:
                    data=os.read(p.stdout.fileno(),4096)
                    if not data:raise RuntimeError('QEMU stopped early: '+result.decode(errors='replace'))
                    result.extend(data)
            pos=len(result);p.stdin.write(cmd.encode()+b'\n');p.stdin.flush()
        tail,_=p.communicate(timeout=timeout);result.extend(tail)
        if p.returncode!=33:raise RuntimeError(f'QEMU return {p.returncode}: '+result.decode(errors='replace'))
        return result.decode(errors='replace').replace('\r','')
    finally:
        if p.poll() is None:p.kill();p.wait()
if __name__=='__main__':
    img=ROOT/'build/qemu-test.img';shutil.copyfile(ROOT/'build/multios.img',img)
    log=run_qemu(img,['ver','dir','unlock YES','md TEST','write /TEST/hello.txt "Hello QEMU"','type /TEST/hello.txt','exit'],sys.argv[1] if len(sys.argv)>1 else 'qemu64')
    (ROOT/'build/qemu-smoke.log').write_text(log);print(log)
