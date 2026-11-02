/* MultiOS — ignored recoverable write failures may NEVER commit a prefix. */
#define main undo_baseline_main
#include "undo_test.c"
#undef main
int main(void){
 high=0;init();CK(!multi_undo_begin(&j));
 for(u32 i=0;i<j.capacity;i++)CK(!multi_undo_write(&j,cfg.data_start+i,a));
 CK(multi_undo_write(&j,cfg.data_start+j.capacity,a)==NOSPACE);
 u32 before=steps;int r=multi_undo_commit(&j);
 printf("commit after ignored capacity error=%d (must be NOSPACE)\n",r);
 CK(r==NOSPACE&&j.active&&steps==before);
 CK(multi_undo_write(&j,cfg.data_start,b)==NOSPACE&&steps==before);
 CK(!multi_undo_recover(&j)&&!j.active&&image_same(stable,base));
 CK(!multi_undo_begin(&j)&&!multi_undo_write(&j,cfg.data_start,b)&&!multi_undo_commit(&j));
 for(u32 reason=0;reason<3;reason++){
  init();CK(!multi_undo_begin(&j)&&!multi_undo_write(&j,cfg.data_start,a));
  int want=reason==2?INVALID:BAD;
  CK(multi_undo_write(&j,reason==0?cfg.journal_start:reason==1?cfg.data_start+cfg.data_sectors:cfg.data_start,reason==2?0:b)==want);
  before=steps;CK(multi_undo_commit(&j)==want&&steps==before);
  CK(!multi_undo_recover(&j)&&image_same(stable,base));
 }
 /* Read/preflight refusals must also forbid committing an earlier write. */
 for(u32 reason=0;reason<5;reason++){
  init();CK(!multi_undo_begin(&j)&&!multi_undo_write(&j,cfg.data_start,a));
  int want=reason==0?BAD:reason==1||reason==3?INVALID:IO;
  int got=reason==0?multi_undo_read(&j,cfg.journal_start,b):reason==1?multi_undo_read(&j,cfg.data_start,0):multi_undo_abort(&j,reason==2?IO:reason==3?0:7);
  CK(got==want);before=steps;
  CK(multi_undo_abort(&j,BAD)==want&&multi_undo_commit(&j)==want&&steps==before);
  CK(!multi_undo_recover(&j)&&image_same(stable,base));
 }
 /* Lost in-memory abort state after reboot cannot turn ACTIVE into commit. */
 init();CK(!multi_undo_begin(&j)&&!multi_undo_write(&j,cfg.data_start,a));
 CK(multi_undo_write(&j,cfg.journal_start,b)==BAD);CK(!multi_block64_flush(&io));
 reboot();CK(!multi_undo_open(&j,&io,&cfg)&&multi_undo_commit(&j)==INVALID);
 CK(!multi_undo_recover(&j)&&image_same(stable,base));
 puts("PASS abort-only transaction: NOSPACE/range/null rejection, no extra target writes/commit, full rollback, fresh transaction, restart recovery");return 0;
}
