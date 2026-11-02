/* MultiOS — malformed modular callbacks fail closed instead of claiming success. */
#define main epoch_baseline_main
#include "file64_epoch_test.c"
#undef main
static void fresh(void){mzero(&vol,sizeof vol);mount_result=read_result=next_result=free_result=mount_bad=next_stall=result=0;assert(!fs_mount()&&!fs_unlock());}
int main(void){
 fresh();mount_result=7;assert(fs_mount()==IO&&!vol.mounted&&!vol.writable);
 fresh();mount_bad=1;assert(fs_mount()==BAD&&!vol.mounted);
 for(int op=0;op<7;op++){
  fresh();result=7;
  int r=op==0?fs_put("/file",(const u8*)"x",1,1,0):op==1?fs_remove("/file",0):op==2?fs_rename("/file","new"):op==3?fs_write64("/file",0,(const u8*)"x",1):op==4?fs_resize64("/file",2):op==5?fs_stream_change("/file","Notes",MULTIOS_STREAM_CREATE,0,0,0):fs_link("/file","/new");
  assert(r==IO&&!vol.mounted&&!vol.writable&&vol.poisoned);
 }
 for(int op=0;op<5;op++){
  fresh();Stream root=fs_root();MultiFile64 f;assert(!fs_open64("/file",&f));u8 b;u32 n=0;Entry e;int r;
  if(op==0){read_result=3;r=fs_read64(&f,0,&b,1);}
  else if(op==1){read_result=3;r=fs_read(f.data,0,&b,1);}
  else if(op==2){free_result=3;r=fs_free(&n);}
  else if(op==3){next_result=3;r=fs_next(root,&n,&e);}
  else {next_stall=1;r=fs_next(root,&n,&e);}
  assert(r==IO&&!vol.mounted&&!vol.writable);
  assert(fs_next(root,&n,&e)==BAD&&fs_read(f.data,0,&b,1)==BAD&&fs_free(&n)==BAD);
 }
 puts("PASS modular FS status contract: mount result/geometry, 7 mutation callbacks, read32/read64/free, invalid/stalled enumeration, unmounted dispatch guards");return 0;
}
