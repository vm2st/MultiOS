#!/usr/bin/env python3
"""MultiOS | vm2 studios | t.me/v2vproj. Standard Python 3 only.
Edit settings in a NEW regular image file. Never write physical disks.
"""
import argparse,os,stat,struct,zlib,shutil,json
from pathlib import Path
CHOICES={'osk':('off','on'),'bios_input':('auto','ps2-only','usb-only'),'storage':('guarded','readonly'),'bios_disk':('ata','usb-bot')}
DEFAULT={k:v[0] for k,v in CHOICES.items()}
PRESETS={'desktop':DEFAULT,'two-buttons':dict(DEFAULT,osk='on'),'safe':dict(DEFAULT,storage='readonly'),'ps2':dict(DEFAULT,bios_input='ps2-only')}
FILES=('EASY/SETTINGS.INI','MULTIOS/BOOT.CFG','MULTIOS/BIOS.CFG')
crc=lambda b:zlib.crc32(b)&0xffffffff
u32=lambda b,o:struct.unpack_from('<I',b,o)[0]
u64=lambda b,o:struct.unpack_from('<Q',b,o)[0]
def require(ok,msg):
 if not ok:raise ValueError(msg)
def settings(text):
 require(len(text)<=4096,'Settings too long');s={}
 for line in text.splitlines():
  line=line.strip()
  if not line or line.startswith(('#',';')):continue
  k,sep,v=line.partition('=');k=k.strip();v=v.strip()
  require(sep and k in CHOICES and k not in s and v in CHOICES[k],'Invalid/duplicate setting: '+line);s[k]=v
 require(set(s)>={'osk','bios_input','storage'},'Required: osk, bios_input, storage');s.setdefault('bios_disk','ata');return s
def settings_text(s):return '# MultiOS | vm2 studios | t.me/v2vproj\n# Edit, then apply to a NEW image with customize.py.\n'+''.join(k+'='+s[k]+'\n' for k in CHOICES)
def profiles(s):
 settings(settings_text(s));ro=s['storage']=='readonly'
 block='pc-usb-bot' if s.get('bios_disk','ata')=='usb-bot' else 'pc-ata-pio'
 bios=f'output=pc-vga-com1\ninput=pc-input-auto\ninput.options={s["bios_input"]}\nblock={block}'+('-ro' if ro else '')+f'\npower=pc-8042-power\nosk={s["osk"]}\n'
 efi='output=uefi-text\ninput=uefi-keys\nblock=uefi-boot-disk'+('-ro' if ro else '')+f'\npower=uefi-reset\nosk={s["osk"]}\n';return bios,efi
def padded(t):
 b=t.encode('ascii');require(len(b)<=512,'Config too long');return b+b'\n'*(512-len(b))
def slot(t):
 b=t.encode('ascii');require(0<len(b)<=492,'BIOS config too long');a=bytearray(512);a[:8]=b'MPCFG25\0';struct.pack_into('<II',a,8,len(b),1);a[16:16+len(b)]=b;struct.pack_into('<I',a,508,crc(a[:508]));return bytes(a)
def decode_slot(a):
 n=u32(a,8);require(a[:8]==b'MPCFG25\0' and u32(a,12)==1 and 0<n<=492 and not any(a[16+n:508]) and crc(a[:508])==u32(a,508),'Invalid BIOS configuration');return a[16:16+n].decode('ascii')
class ESP:
 """Bounded FAT32 reader/ownership validator, not a FAT allocator or repair tool."""
 def __init__(self,b):
  self.b=b;require(len(b)>=512 and b[510:512]==b'\x55\xaa','ESP signature')
  self.spc=b[13];reserved=struct.unpack_from('<H',b,14)[0];nf=b[16];nfsec=u32(b,36)
  require(struct.unpack_from('<H',b,11)[0]==512 and self.spc in (1,2,4,8,16,32,64,128) and nf==2 and reserved>=1 and not struct.unpack_from('<H',b,17)[0] and not struct.unpack_from('<H',b,22)[0] and not struct.unpack_from('<H',b,40)[0] and not struct.unpack_from('<H',b,42)[0],'Unsupported ESP geometry')
  total=u32(b,32);self.data=(reserved+nf*nfsec)*512;self.cs=self.spc*512
  require(total*512==len(b) and 0<self.data<len(b),'ESP length');self.count=(len(b)-self.data)//self.cs;self.fat=b[reserved*512:(reserved+nfsec)*512]
  require(self.count>=65525 and len(self.fat)//4>=self.count+2 and self.fat==b[(reserved+nfsec)*512:(reserved+2*nfsec)*512],'ESP FAT mirrors/capacity')
  self.owned=set();self.files={};self.walk(u32(b,44),'',0)
 def chain(self,c):
  a=[]
  while c<0xffffff8:
   require(2<=c<self.count+2 and c not in self.owned,'Invalid/looped/cross-linked ESP chain');self.owned.add(c);a.append(c);c=u32(self.fat,c*4)&0xfffffff
  require(a,'Empty chain');return a
 def offset(self,c):return self.data+(c-2)*self.cs
 def walk(self,c,path,depth):
  require(depth<24 and len(self.files)<10000,'ESP tree limit');cc=self.chain(c);raw=b''.join(self.b[self.offset(k):self.offset(k)+self.cs] for k in cc)
  for o in range(0,len(raw),32):
   e=raw[o:o+32]
   if e[0]==0:break
   if e[0]==0xe5 or e[11]==15 or e[11]&8 or e[:1]==b'.':continue
   name=e[:8].decode('ascii').rstrip();ext=e[8:11].decode('ascii').rstrip();full=path+name+('.'+ext if ext else '')
   require(full not in self.files,'Duplicate file');k=(struct.unpack_from('<H',e,20)[0]<<16)|struct.unpack_from('<H',e,26)[0];n=u32(e,28)
   if e[11]&16:self.walk(k,full+'/',depth+1)
   else:
    cc=self.chain(k) if k else [];require(n<=len(cc)*self.cs,'Truncated file');self.files[full]=(cc,n)
 def read(self,path):
  require(path in self.files,'Missing factory config: '+path);cc,n=self.files[path];return b''.join(self.b[self.offset(k):self.offset(k)+self.cs] for k in cc)[:n]
 def patches(self,path,data):
  cc,n=self.files[path];require(n==512 and len(data)==512 and cc,'Expected fixed-size factory config');return [(self.offset(cc[0]),data)]
def read_at(f,off,n):
 f.seek(off);b=f.read(n);require(len(b)==n,'Truncated image');return b
def inspect_file(f):
 size=os.fstat(f.fileno()).st_size;require(size%512==0 and 2048*512<size<=1024**3,'Expected factory image <=1GiB')
 m=read_at(f,0,512);require(m[510:]==b'\x55\xaa' and m[432:440]==b'MP22\x10\0\x01\0','Unknown image format')
 h=read_at(f,17*512,512);require(h[:8]==b'MPRW22\0\0' and h[24:28]==b'MC25','Requires 0.25 image capability');n=u32(h,8);require(16<=n<=(2048-18)*512 and crc(read_at(f,18*512,n))==u32(h,12),'Kernel CRC')
 foot=read_at(f,size-1024,1024);a=foot[:512]
 require(a==foot[512:] and a[:8]==b'MPSTORE1' and crc(a[:508])==u32(a,508) and u32(a,76)==crc(m) and u32(a,8)==1 and u32(a,12)==512 and u64(a,16)==size//512 and not any(a[80:508]),'Storage footer/binding')
 start=u64(a,24);sectors=u64(a,32);require(4<=sectors<=8194 and sectors%2==0 and start+sectors+2==size//512,'Journal extent')
 parts=[]
 for i in range(4):
  e=m[446+16*i:462+16*i];t=e[4];base=u32(e,8);count=u32(e,12)
  if t:require(base>=2048 and count>0 and base+count<=size//512,'Partition extent');parts.append((t,base,count))
 for i,(_,base,count) in enumerate(parts):
  for _,b,c in parts[:i]:require(base>=b+c or b>=base+count,'Partition overlap')
 require((0xda,start,sectors+2) in parts,'Missing journal partition');h=read_at(f,start*512,1024)
 require(h[:512]==h[512:] and h[:8]==b'MPUNDO1\0' and crc(h[:508])==u32(h,508) and u32(h,20)==0,'Journal not clean/equal; recover in MultiOS first')
 require(u32(h,8)==1 and u32(h,12)==512 and u32(h,16)==512 and u32(h,40)==0 and u32(h,44)==(sectors-2)//2 and u64(h,24)>0 and u64(h,32)<=u64(h,24) and not any(h[104:508]),'Journal fields')
 require(tuple(u64(h,i) for i in (48,56,64,72,80))==(start,sectors,u64(a,40),u64(a,48),size//512) and h[88:104]==a[56:72],'Journal identity/geometry')
 part=u32(a,72);require(1<=part<=4,'Protected partition index');e=m[446+(part-1)*16:462+(part-1)*16]
 require(u32(e,8)==u64(a,40) and u32(e,12)==u64(a,48) and e[4] not in (0,0xda,0xee,5,0xf,0x85),'Protected data extent')
 require(parts[0][0] in (0xef,0xb,0xc) and parts[0][1:]==(2048,131072),'Expected factory ESP')
 esp=ESP(read_at(f,1048576,67108864));s=settings(esp.read(FILES[0]).decode('ascii'));bios,efi=profiles(s)
 require(decode_slot(read_at(f,8192,512))==bios and esp.read(FILES[1])==padded(efi) and esp.read(FILES[2])==padded(bios),'Configuration mirrors differ');return esp,s
def open_image(path):
 path=Path(path);require(not path.is_symlink(),'Symlink refused');fd=os.open(path,os.O_RDONLY|getattr(os,'O_NOFOLLOW',0)|getattr(os,'O_NONBLOCK',0))
 try:
  st=os.fstat(fd);require(stat.S_ISREG(st.st_mode) and st.st_nlink==1,'Only regular single-link files, never devices');f=os.fdopen(fd,'rb');fd=-1
  if os.name=='posix':
   import fcntl
   fcntl.flock(f,fcntl.LOCK_SH|fcntl.LOCK_NB);fcntl.lockf(f,fcntl.LOCK_SH|fcntl.LOCK_NB)
  elif os.name=='nt':
   import msvcrt
   msvcrt.locking(f.fileno(),msvcrt.LK_NBRLCK,1)
  return f
 except BaseException:
  if fd>=0:os.close(fd)
  else:f.close()
  raise
def apply(source,output,s):
 bios,efi=profiles(s);output=Path(output);require(not output.exists() and not output.is_symlink(),'Output must be NEW');made=False
 with open_image(source) as f:
  try:
   before=os.fstat(f.fileno());esp,_=inspect_file(f)
   with output.open('xb') as out:
    made=True;f.seek(0);shutil.copyfileobj(f,out,1024**2);patches=[(8192,slot(bios))]
    for path,data in zip(FILES,(padded(settings_text(s)),padded(efi),padded(bios))):patches.extend((1048576+off,b) for off,b in esp.patches(path,data))
    for off,b in patches:out.seek(off);out.write(b)
    out.flush();os.fsync(out.fileno())
   after=os.fstat(f.fileno());require((before.st_size,before.st_mtime_ns,before.st_ctime_ns)==(after.st_size,after.st_mtime_ns,after.st_ctime_ns),'Source changed during copy')
   with open_image(output) as check:require(inspect_file(check)[1]==s,'Readback differs')
  except BaseException:
   if made:output.unlink(missing_ok=True)
   raise
 return output
def gui():
 import tkinter as tk
 from tkinter import ttk,filedialog,messagebox
 root=tk.Tk();root.title('MultiOS — vm2 studios | t.me/v2vproj');root.geometry('660x370');path=tk.StringVar();v={k:tk.StringVar(value=x) for k,x in DEFAULT.items()}
 ttk.Label(root,text='MultiOS — настройки НОВОЙ копии образа',font=('Arial',14)).pack(pady=12)
 ttk.Label(root,text='Закройте VM. Исходный образ не изменяется. Python 3 + Tkinter.').pack();ttk.Entry(root,textvariable=path,width=85).pack(pady=8)
 def choose():
  p=filedialog.askopenfilename(filetypes=[('MultiOS image','*.img')])
  if not p:return
  try:
   with open_image(p) as f:_,s=inspect_file(f)
   path.set(p)
   for k in v:v[k].set(s[k])
  except Exception as e:messagebox.showerror('Проверка образа',str(e))
 ttk.Button(root,text='Открыть .img',command=choose).pack();form=ttk.Frame(root);form.pack(pady=12)
 labels={'osk':'Экранная клавиатура','bios_input':'Ввод BIOS','storage':'guarded: RO до UNLOCK; readonly: запись запрещена','bios_disk':'Системный диск BIOS (UEFI не меняется)'}
 for i,k in enumerate(CHOICES):
  ttk.Label(form,text=labels[k]).grid(row=i,column=0,sticky='w',pady=3);ttk.Combobox(form,textvariable=v[k],values=CHOICES[k],state='readonly',width=15).grid(row=i,column=1)
 def save():
  if not path.get():return messagebox.showerror('Образ','Выберите исходный .img')
  p=filedialog.asksaveasfilename(defaultextension='.img',initialfile='MultiOS-custom.img')
  if not p:return
  try:apply(path.get(),p,{k:x.get() for k,x in v.items()});messagebox.showinfo('Готово','Новая копия сохранена и проверена.\n'+p)
  except Exception as e:messagebox.showerror('Не сохранено',str(e))
 ttk.Button(root,text='Сохранить НОВЫЙ образ…',command=save).pack(pady=8);ttk.Label(root,text='UNPUBLISHED development • НЕ стабильный Release 1.0').pack();root.mainloop()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--gui',action='store_true');p.add_argument('--list-presets',action='store_true');p.add_argument('image',nargs='?');p.add_argument('--output');g=p.add_mutually_exclusive_group();g.add_argument('--settings');g.add_argument('--preset',choices=PRESETS);a=p.parse_args()
 if a.gui:return gui()
 if a.list_presets:print(json.dumps(PRESETS,indent=2));return
 require(a.image,'Specify IMAGE, --gui or --list-presets')
 if a.output:
  require(a.settings or a.preset,'Choose --settings FILE or --preset NAME');s=settings(Path(a.settings).read_text(encoding='ascii')) if a.settings else dict(PRESETS[a.preset]);print('Saved and validated:',apply(a.image,a.output,s))
 else:
  require(not a.settings and not a.preset,'Use --output NEW.img')
  with open_image(a.image) as f:_,s=inspect_file(f)
  print(settings_text(s),end='')
if __name__=='__main__':
 try:main()
 except (Exception,KeyboardInterrupt) as e:raise SystemExit('Not modified: '+str(e))
