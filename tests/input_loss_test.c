/* A lost input byte must never turn a command into a different executable line. */
#include "multios/hal.h"
#include "multios/hid.h"
#include "multios/editor.h"
#include <assert.h>
#include <stdio.h>
Volume vol;
void putch(char c){(void)c;}
static MultiHal hal;
const MultiHal *multi_hal(void){return &hal;}
static void panel(void*c,int a,int b,int d,int e){(void)c;(void)a;(void)b;(void)d;(void)e;}
int main(void){
 MultiHid h;MultiEdit e;u8 r[8]={0};multi_hid_reset(&h);multi_edit_reset(&e);
 assert(multi_edit_load(&e,"del important")==0);
 for(int i=0;i<16;i++){r[2]=0;assert(multi_hid_report(&h,r,8)==0);r[2]=i==0?40:4;assert(multi_hid_report(&h,r,8)==0);}
 assert(h.overflows==1);
 int k=multi_hid_get(&h);assert(k==MULTI_KEY_HISTORY_NEXT+1);
 assert(multi_osk_process(k)==k);assert(multi_edit_key(&e,k)==0&&e.overflow);
 assert(multi_hid_get(&h)==-2); /* queued Enter was discarded */
 assert(multi_edit_key(&e,'\n')==MULTI_EDIT_SUBMIT&&e.overflow);
 assert(multi_edit_key(&e,3)==MULTI_EDIT_CANCEL&&!e.overflow&&!e.length);
 hal.console.osk=panel;assert(multi_osk_enable(1)==0);
 assert(multi_osk_process(k)==k);assert(multi_edit_key(&e,k)==0&&e.overflow);
 assert(multi_edit_key(&e,'x')==MULTI_EDIT_CHANGED&&e.overflow);
 multi_hid_reset(&h);
 for(int i=0;i<14;i++){r[2]=0;multi_hid_report(&h,r,8);r[2]=4;multi_hid_report(&h,r,8);}
 r[2]=0;multi_hid_report(&h,r,8);r[2]=5;r[3]=6;r[4]=57;r[5]=7;
 assert(multi_hid_report(&h,r,8)==0&&h.overflows==1&&h.caps==1);
 assert(multi_hid_get(&h)==MULTI_KEY_INPUT_LOST&&multi_hid_get(&h)==-2);
 puts("PASS input-loss pipeline: HID overflow discards queued Enter, OSK forwards loss, editor blocks submission until cancellation");
}
