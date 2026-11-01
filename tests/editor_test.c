/* vm2 studios | t.me/v2vproj — bounded editor compared with a libc reference. */
#include "multios/editor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void out(const char*s){(void)s;}void putch(char c){(void)c;}
int main(void){
 MultiEdit e;multi_edit_reset(&e);char ref[512]="";u32 n=0,pos=0,seed=123;
 for(int trial=0;trial<100000;trial++){
  seed=seed*1664525u+1013904223u;int key;
  switch(seed%8){
  case 0:key=2;if(pos)pos--;break;
  case 1:key=6;if(pos<n)pos++;break;
  case 2:key=1;pos=0;break;
  case 3:key=5;pos=n;break;
  case 4:key=4;if(pos<n){memmove(ref+pos,ref+pos+1,n-pos);n--;}break;
  case 5:key=8;if(pos){memmove(ref+pos-1,ref+pos,n-pos+1);pos--;n--;}break;
  default:key='a'+(seed>>16)%26;if(n<511){memmove(ref+pos+1,ref+pos,n-pos+1);ref[pos++]=key;n++;}break;
  }
  multi_edit_key(&e,key);assert(e.cursor==pos&&e.length==n);assert(!strcmp(e.text,ref));
 }
 assert(multi_edit_key(&e,3)==MULTI_EDIT_CANCEL&&e.length==0);assert(multi_edit_load(&e,"echo test")==0);
 assert(multi_edit_key(&e,'\n')==MULTI_EDIT_SUBMIT);assert(multi_edit_load(&e,"bad\nline")==INVALID);
 char large[514];memset(large,'x',513);large[513]=0;assert(multi_edit_load(&e,large)==TOOLONG);
 assert(!strcmp(e.text,"echo test"));multi_edit_reset(&e);
 for(int i=0;i<512;i++)multi_edit_key(&e,'x');assert(e.length==511&&e.overflow);assert(e.text[511]==0);
 assert(!multi_edit_load(&e,"AПривет😀Z"));u32 before=e.length;
 multi_edit_key(&e,2);multi_edit_key(&e,8);assert(!strcmp(e.text,"AПриветZ")&&e.length==before-4);
 multi_edit_key(&e,2);multi_edit_key(&e,4);assert(!strcmp(e.text,"AПривеZ"));
 multi_edit_reset(&e);const char *u="Ж😀";for(u32 i=0;u[i];i++)multi_edit_key(&e,(u8)u[i]);assert(!strcmp(e.text,u));
 assert(multi_edit_load(&e,"\xc0\xaf")==INVALID);assert(!strcmp(e.text,u));
 multi_edit_key(&e,0xe0);assert(multi_edit_key(&e,'\n')==MULTI_EDIT_SUBMIT&&e.overflow);
 puts("Editor: 100000 reference-model events, insertion/deletion/cursor, load validation, overflow, cancel and submit passed.");
}
