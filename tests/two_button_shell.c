/* vm2 studios | t.me/v2vproj — real shell/FS driven ONLY by NEXT/SELECT. */
#include "multios/hal.h"
#include "multios/osk.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
static FILE *image;
static unsigned steps,offset;
static const char script[]="ver\necho two buttons\nosk off\necho reopened\nunlock YES\nmd TWO\nwrite TWO/HELLO.TXT \"Hello Two Buttons!\"\ntype TWO/HELLO.TXT\nlock\nexit\n";
static int readsector(void *c,u32 l,u8 *b){(void)c;return fseek(image,(long)l*512,SEEK_SET)||fread(b,1,512,image)!=512?IO:0;}
static int writesector(void *c,u32 l,const u8 *b){(void)c;return fseek(image,(long)l*512,SEEK_SET)||fwrite(b,1,512,image)!=512?IO:0;}
static int flush(void *c){(void)c;return fflush(image)?IO:0;}
static void put(void *c,char ch){(void)c;putchar(ch);}
static void paint(void *c,int on,int r,int col,int sh){(void)c;(void)on;(void)r;(void)col;(void)sh;}
static int key(void *c){
 (void)c;assert(++steps<50000);assert(script[offset]);
 if(!multi_osk_enabled())return MULTI_KEY_SELECT;
 int r,col,sh,tr=-1,tc=-1,targetsh=0;multi_osk_position(&r,&col,&sh);
 int wanted=(unsigned char)script[offset];
 if(wanted=='\n'){tr=4;tc=2;targetsh=sh;}
 else if(wanted==' '){tr=4;tc=0;targetsh=sh;}
 else for(int ss=0;ss<2&&tr<0;ss++)for(int rr=0;rr<4&&tr<0;rr++)for(int cc=0;cc<12;cc++)
  if((unsigned char)multi_osk_label(rr,cc,ss)[0]==wanted){tr=rr;tc=cc;targetsh=ss;break;}
 assert(tr>=0);
 if(targetsh!=sh){tr=4;tc=3;}
 if(r!=tr||col!=tc)return MULTI_KEY_NEXT;
 if(targetsh==sh)offset++;
 return MULTI_KEY_SELECT;
}
static void stop(void *c){(void)c;}
int main(int argc,char **argv){
 assert(argc==2);image=fopen(argv[1],"r+b");assert(image);
 assert(!fseek(image,0,SEEK_END));long size=ftell(image);assert(size>0&&size%512==0);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="two-button shell test",.architecture="host test, not BSP",
 .block={.sectors=(u32)(size/512),.sector_bytes=512,.writable=1,.read=readsector,.write=writesector,.flush=flush},
 .console={.putc=put,.getc=key,.osk=paint},.stop=stop};
 assert(multi_hal_bind(&h)==0);shell();assert(offset==sizeof(script)-1);assert(!vol.poisoned);
 fclose(image);fprintf(stderr,"Two-button shell passed: %u NEXT/SELECT events, no ASCII/arrow input; real exFAT write/read.\n",steps);
}
