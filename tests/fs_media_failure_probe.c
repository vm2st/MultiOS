/* MultiOS: selected-media notification must stop even success-returning callbacks.
 * Raw two-sector transactions are restored before any filesystem validation.
 * Not a real USB removal or power-loss certification. */
#define main readonly_probe_main
#include "fs_read_probe.c"
#undef main
#include "multios/storage_tx.h"
#include <string.h>
static int cookie,armed,fired,phase;static u32 calls,fail_at;
static u32 base,extent;static u8 old[2][512],next[2][512],check[2][512];
static int notify(int after){
 if(armed&&calls==fail_at&&phase==after){assert(!fired);fired=1;assert(!multi_hal_media_failed(&cookie));return 1;}return 0;
}
static int provider_read(void*x,u32 l,u8*b){assert(x==&cookie);calls++;if(notify(0))return 0;int r=rd(0,l,b);notify(1);return r;}
static int provider_write(void*x,u32 l,const u8*b){assert(x==&cookie);calls++;if(notify(0))return 0;int r=l>=sectors||fseeko(file,(off_t)l*512,SEEK_SET)?IO:fwrite(b,1,512,file)==512?0:IO;notify(1);return r;}
static int provider_flush(void*x){assert(x==&cookie);calls++;if(notify(0))return 0;int r=fflush(file)?IO:0;notify(1);return r;}
static void binddisk(void){vol.mounted=vol.writable=0;MultiHal h={.abi=MULTIOS_HAL_ABI,.name="media failure model",.architecture="host",.block={.ctx=&cookie,.sectors=sectors,.sector_bytes=512,.writable=1,.read=provider_read,.write=provider_write,.flush=provider_flush},.console={.putc=put,.getc=get}};assert(!multi_hal_bind(&h));}
static void mountdisk(void){binddisk();assert(!fs_mount()&&!fs_unlock());}
static int transaction(int commit){int r=multi_storage_begin();if(r)return r;
 r=disk_write(base,next[0]);if(!r)r=disk_write(base+1,next[1]);
 if(!commit&&!r)return 0;return multi_storage_finish(r);
}
static void no_more_io(void){u32 before=calls;u8 b[512];assert(!disk_writable());
 assert(disk_read(base,b)<0&&disk_write(base,old[0])<0&&disk_flush()<0);
 assert(multi_storage_read(base,b)<0&&multi_storage_write(base,old[0])<0&&multi_storage_flush()<0);
 assert(multi_storage_prepare()<0&&multi_storage_begin()<0&&multi_storage_finish(0)<0);
 assert(calls==before);
 /* A bookkeeping reset is not a physical reopen and must not heal the HAL. */
 multi_storage_reset();assert(multi_storage_blocked());
 assert(multi_storage_read(base,b)<0&&multi_storage_flush()<0&&multi_storage_prepare()<0);
 assert(calls==before);
}
/* Deliberately reopen after simulated removal. Completion may be unknown near
 * the CLEAN commit point; require ALL old or ALL new, never a mixed prefix. */
static void reopen_restore(int old_only){armed=0;binddisk();assert(!multi_storage_prepare());
 assert(!disk_read(base,check[0])&&!disk_read(base+1,check[1]));
 int before=!memcmp(check,old,sizeof old),after=!memcmp(check,next,sizeof next);
 assert(before||(!old_only&&after));vol.base=base;vol.sectors=extent;
 assert(!multi_storage_begin()&&!disk_write(base,old[0])&&!disk_write(base+1,old[1])&&!multi_storage_finish(0));
 mountdisk();
}
int main(int argc,char**argv){assert(argc==2);file=fopen(argv[1],"r+b");assert(file&&!fseeko(file,0,SEEK_END));sectors=(u32)(ftello(file)/512);mountdisk();base=vol.base;extent=vol.sectors;
 assert(!disk_read(base,old[0])&&!disk_read(base+1,old[1]));memcpy(next,old,sizeof old);next[0][99]^=0x5a;next[1][107]^=0xa5;
 /* Notification outside a callback must also invalidate raw storage/flush. */
 assert(!multi_hal_media_failed(&cookie));u32 previous=calls;int r=disk_flush();
 printf("flush after selected-media failure=%d callbacks=%u (must fail without callback)\n",r,calls-previous);fflush(stdout);
 assert(r<0&&calls==previous);no_more_io();reopen_restore(1);
 calls=0;assert(!transaction(1));u32 total=calls;reopen_restore(0);
 u32 attempts=0;
 for(phase=0;phase<2;phase++)for(fail_at=1;fail_at<=total;fail_at++){
  calls=0;fired=0;armed=1;r=transaction(1);assert(fired&&r<0);no_more_io();reopen_restore(0);attempts++;
 }
 /* Leave a durable ACTIVE transaction; notify on every reopen/recovery I/O. */
 assert(!transaction(0)&&!disk_flush());binddisk();calls=0;assert(!multi_storage_prepare());u32 recovery_calls=calls;reopen_restore(1);
 u32 recoveries=0;
 for(phase=0;phase<2;phase++)for(fail_at=1;fail_at<=recovery_calls;fail_at++){
  assert(!transaction(0)&&!disk_flush());binddisk();calls=0;fired=0;armed=1;r=multi_storage_prepare();assert(fired&&r<0);no_more_io();reopen_restore(1);recoveries++;
 }
 assert(!fs_put("/KEEP.TXT",(const u8*)"KEEP!",5,0,0));MultiFile64 f;u8 b[5];assert(!fs_open64("/KEEP.TXT",&f)&&!fs_read64(&f,0,b,5)&&!memcmp(b,"KEEP!",5)&&!disk_flush());fclose(file);
 printf("PASS selected-media failure: %u transaction + %u reopen/recovery callback boundaries (before/after, false success); no I/O after notification, atomic old/new outcome, repeatable recovery and fresh FS RW\n",attempts,recoveries);return 0;
}
