/* vm2 studios | t.me/v2vproj — independent protocol tests, no USB hardware. */
#include "multios/hid.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void *mzero(void *p,u32 n){return memset(p,0,n);}void mcopy(void *d,const void *s,u32 n){memmove(d,s,n);}
static MultiHid h;
static int stroke(int usage,int modifiers){u8 r[8]={0};multi_hid_report(&h,r,8);r[0]=modifiers;r[2]=usage;assert(multi_hid_report(&h,r,8)==0);return multi_hid_get(&h);}
int main(void){
 multi_hid_reset(&h);assert(multi_hid_get(&h)==-2);assert(multi_hid_report(0,0,0)==INVALID);
 for(int i=0;i<26;i++){assert(stroke(4+i,0)=='a'+i);assert(stroke(4+i,2)=='A'+i);assert(stroke(4+i,1)==i+1);}
 for(int i=0;i<10;i++){assert(stroke(30+i,0)=="1234567890"[i]);assert(stroke(30+i,0x20)=="!@#$%^&*()"[i]);}
 for(int i=0;i<12;i++){assert(stroke(45+i,0)=="-=[]\\#;'`,./"[i]);assert(stroke(45+i,2)=="_+{}|~:\"~<>?"[i]);}
 assert(stroke(57,0)==-2);assert(stroke(4,0)=='A');assert(stroke(4,2)=='a');assert(stroke(57,0)==-2);
 assert(stroke(83,0)==-2);for(int i=0;i<11;i++)assert(stroke(89+i,0)=="1234567890."[i]);
 assert(stroke(63,0)==MULTI_KEY_NEXT);assert(stroke(64,0)==MULTI_KEY_SELECT);assert(stroke(62,0)==MULTI_KEY_PREVIOUS);
 assert(stroke(59,0)==MULTI_KEY_OSK);assert(stroke(74,0)==MULTI_KEY_HOME);assert(stroke(76,0)==MULTI_KEY_DELETE);
 u8 report[8]={0,0,4,4,5,6};multi_hid_reset(&h);assert(multi_hid_report(&h,report,8)==0);
 assert(multi_hid_get(&h)=='a');assert(multi_hid_get(&h)=='b');assert(multi_hid_get(&h)=='c');assert(multi_hid_get(&h)==-2);
 assert(multi_hid_report(&h,report,8)==0);assert(multi_hid_get(&h)==-2);
 report[2]=1;assert(multi_hid_report(&h,report,8)==UNSUP);assert(h.rollovers==1);assert(multi_hid_get(&h)==-2);
 assert(multi_hid_report(&h,report,7)==INVALID);assert(multi_hid_report(&h,report,9)==INVALID);
 multi_hid_reset(&h);u32 seed=71;
 for(int trial=0;trial<50000;trial++){
  for(int i=0;i<8;i++){seed=seed*1664525u+1013904223u;report[i]=seed>>24;}
  multi_hid_report(&h,report,8);assert(h.head<MULTI_HID_QUEUE&&h.tail<MULTI_HID_QUEUE);
  if(trial%3==0)while(multi_hid_get(&h)!=-2){}
 }
 puts("HID: US letters/modifiers, numbers, punctuation, caps/num, keypad, OSK/editor events, 6KRO, duplicate/held/error/queue handling; 50000 fuzz reports passed.");
}
