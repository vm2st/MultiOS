/* vm2 studios | t.me/v2vproj — regression: repeated failure of mirrored headers.
 * Reuses durable/cache model, NOT physical power-loss certification. */
#define main old_undo_main
#include "undo_test.c"
#undef main
static int try_begin(void){if(setjmp(poweroff))return IO;return multi_undo_begin(&j);}
static int regression(void){
 high=0;init();CK(!transaction(0));
 reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=6;mode=3;CK(try_recover()<0&&hit);
 reboot();CK(!multi_undo_open(&j,&io,&cfg)&&!j.active);
 steps=0;cut=2;mode=3;CK(try_begin()<0&&hit);
 reboot();int rc=multi_undo_open(&j,&io,&cfg);
 if(rc){printf("REGRESSION reproduced: second crash made adjacent mirror generations unreadable (rc=%d).\n",rc);return 42;}
 CK(!multi_undo_recover(&j)&&image_same(stable,base));return 0;
}
int main(int argc,char**argv){(void)argv;if(regression())return 42;if(argc>1){puts("PASS repeated-CLEAN regression");return 0;}
 u32 chains=0;
 for(high=0;high<2;high++){
  init();CK(!transaction(1));u32 total=steps;
  /* Every initial transaction write/flush boundary x four persistence models,
   * followed by EVERY next recovery/begin boundary and both error/crash forms. */
  for(u32 first=1;first<=total;first++)for(u32 fm=0;fm<4;fm++){
   memcpy(stable,base,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));steps=0;cut=first;mode=fm;CK(try_tx(1)<0&&hit);
   memcpy(pending,stable,sizeof(pending));
   reboot();CK(!multi_undo_open(&j,&io,&cfg));int active=j.active;steps=0;
   CK(!(active?multi_undo_recover(&j):multi_undo_begin(&j)));u32 second_total=steps;
   for(u32 second=1;second<=second_total;second++)for(u32 sm=0;sm<4;sm++)for(u32 err=0;err<2;err++){
    memcpy(stable,pending,sizeof(stable));reboot();CK(!multi_undo_open(&j,&io,&cfg));CK(j.active==active);
    steps=0;cut=second;mode=sm;return_error=err;CK((active?try_recover():try_begin())<0&&hit);
    recover(0);chains++;
   }
  }
 }
 /* Many consecutive failures after a first durable mirror, alternating slot
  * ownership. A single valid mirror also survives repeated retries. */
 for(u32 only=0;only<3;only++){
  high=0;init();if(only){stable[JS+(only-1)][0]^=1;}
  for(u32 n=0;n<64;n++){
   reboot();CK(!multi_undo_open(&j,&io,&cfg));int active=j.active;CK(!writes&&!flushes);if(only&&n==0)CK(j.degraded&&j.mirror_first==only-1);
   steps=0;cut=2;mode=3;CK((active?try_recover():try_begin())<0&&hit);
  }
  recover(1);
 }
 printf("PASS repeated failures: %u two-fault chains; low and >32-bit LBAs; 3 x 64 consecutive header interruptions incl either sole surviving mirror.\n",chains);
 puts("Exhaustive write/flush pairs for this bounded transaction, not exhaustive hardware/FS states.");return 0;
}
