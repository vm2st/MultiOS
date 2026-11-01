/* vm2 studios | t.me/v2vproj — success-returning silent faults and read errors.
 * Reuses persistent/cache RAM model. No actual hardware certification. */
#define main original_undo_main
#include "undo_test.c"
#undef main
int main(void){u32 silent_cases=0,read_cases=0,recovery_cases=0;
 /* CRC-valid impossible adjacent generations must be rejected without I/O. */
 high=0;
 for(u32 variant=0;variant<5;variant++){
  init();u8*old=stable[JS],*next=stable[JS+1];p64(next+24,2);
  if(variant==0){/* CLEAN -> CLEAN */}
  else {p32(next+20,1);p64(next+32,2);
   if(variant==1)p32(next+40,1); /* begin with nonzero count */
   if(variant==2)p64(next+32,1); /* begin with old transaction */
   if(variant>=3){p32(old+20,1);p64(old+32,1);p64(next+32,variant==3?2:1);p32(next+40,variant==3?1:2);fix(old);}
  }
  fix(next);reboot();CK(multi_undo_open(&j,&io,&cfg)==BAD&&!writes&&!flushes);
 }
 for(high=0;high<2;high++){
  init();CK(!transaction(1));u32 total=writes,nreads=reads;
  for(u32 n=1;n<=total;n++)for(u32 m=0;m<3;m++){
   init();silent_at=n;silent_mode=m;int rc=transaction(1);CK(silent_hit);
   if(!rc)CK(image_same(stable,expected));else{CK(j.failed);u32 before=writes;CK(multi_undo_commit(&j)==READONLY&&writes==before);}
   recover(0);silent_cases++;
  }
  /* Every read boundary in a successful live transaction must fail closed. */
  for(u32 n=1;n<=nreads;n++){
   init();reads=0;read_fail=n;int rc=transaction(1);
   if(rc<0){CK(j.failed&&io.failed);recover(0);read_cases++;}
   else CK(n>reads);
  }
  init();CK(!transaction(0));memcpy(pending,stable,sizeof(pending));
  reboot();CK(!multi_undo_open(&j,&io,&cfg));CK(multi_undo_commit(&j)==INVALID);
  CK(multi_undo_write(&j,cfg.data_start+2,a)==INVALID);CK(!multi_undo_recover(&j));u32 recovery_writes=writes;
  for(u32 n=1;n<=recovery_writes;n++)for(u32 m=0;m<3;m++){
   memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));silent_at=n;silent_mode=m;
   int rc=multi_undo_recover(&j);CK(silent_hit);if(!rc)CK(image_same(stable,base));else CK(j.failed);
   recover(1);recovery_cases++;
  }
 }
 printf("PASS write guards: %u silent write attempts (drop/173-byte partial/bit corruption), %u injected read failures, %u silent recovery-write attempts; low and >32-bit LBA.\n",silent_cases,read_cases,recovery_cases);
 puts("Verified mirrors, before-images, last target CRCs, restored bytes; fresh ACTIVE cannot commit/write. Honest flush/read model only; not lying-cache or hardware certification.");return 0;
}
