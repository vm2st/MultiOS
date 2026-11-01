/* vm2 studios | t.me/v2vproj — exhaustive scalar codec and malformed-input checks. */
#include "unicode.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void out(const char*s){(void)s;}void putch(char c){(void)c;}
int main(void){
 u32 count=0;for(u32 c=1;c<=0x10ffff;c++){
  if(c>=0xd800&&c<=0xdfff)continue;
  u16 source[2],round[2];u32 n=c<65536?1:2,units=99;
  if(n==1)source[0]=c;else{source[0]=0xd800+((c-65536)>>10);source[1]=0xdc00+((c-65536)&1023);}
  char text[5];assert(!multi_utf16_to8(source,n,text,sizeof(text)));
  assert(!multi_utf8_to16(text,round,2,&units)&&units==n&&!memcmp(source,round,n*2));
  char shortbuf[5]="keep";assert(multi_utf16_to8(source,n,shortbuf,(u32)strlen(text))==TOOLONG&&!strcmp(shortbuf,"keep"));count++;
 }
 const char *bad[]={"\xc0\xaf","\xc1\xbf","\xe0\x80\xaf","\xed\xa0\x80","\xed\xbf\xbf","\xf0\x80\x80\xaf","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xff","\x80","\xe2\x82","\xc2","\xe2Z","\xf0\x9f\x98"};
 for(u32 i=0;i<sizeof(bad)/sizeof(bad[0]);i++){u16 out16[4]={1,2,3,4};u32 n=88;assert(multi_utf8_to16(bad[i],out16,4,&n)==INVALID&&n==88&&out16[0]==1);}
 u16 bad16[][2]={{0xd800,0},{0xdc00,0},{0xd800,'A'},{'A',0xdc00},{0,0}};
 for(u32 i=0;i<5;i++){char s[16]="unchanged";assert(multi_utf16_to8(bad16[i],i<2?1:2,s,16)==INVALID&&!strcmp(s,"unchanged"));}
 assert(multi_default_equal("Привет😀.txt","ПРИВЕТ😀.TXT")==1);
 assert(multi_default_equal("αβγ","ΑΒΓ")==1);assert(!multi_default_equal("café","café"));
 assert(multi_name_valid("Привет😀.txt")==0);assert(multi_name_valid("bad?.txt")==INVALID);
 assert(multi_name_valid("x. ")==INVALID);assert(multi_name_valid("\xed\xa0\x80")==INVALID);
 printf("PASS %u Unicode scalar round-trips + atomic capacity checks; 14 malformed UTF-8; 5 invalid UTF-16; Cyrillic/Greek case folding, no normalization.\n",count);
}
