/* vm2 studios | t.me/v2vproj — real shell/FS driven ONLY by NEXT/SELECT. */
#include "multios/hal.h"
#include "multios/osk.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
static FILE *image;
static unsigned offset;static char captured[8192];static u32 captured_size;
static const char script[]="echo SHOULD_NOT_RUN\1\necho RECOVERED\nexit\n";
static int readsector(void *c,u32 l,u8 *b){(void)c;return fseek(image,(long)l*512,SEEK_SET)||fread(b,1,512,image)!=512?IO:0;}
static int writesector(void *c,u32 l,const u8 *b){(void)c;return fseek(image,(long)l*512,SEEK_SET)||fwrite(b,1,512,image)!=512?IO:0;}
static int flush(void *c){(void)c;return fflush(image)?IO:0;}
static void put(void *c,char ch){(void)c;assert(captured_size<sizeof(captured)-1);captured[captured_size++]=ch;}
static void paint(void *c,int on,int r,int col,int sh){(void)c;(void)on;(void)r;(void)col;(void)sh;}
static int key(void *c){(void)c;assert(offset<sizeof(script)-1);int k=script[offset++];return k==1?MULTI_KEY_INPUT_LOST:k;}
static void stop(void *c){(void)c;}
int main(int argc,char **argv){
 assert(argc==2);image=fopen(argv[1],"r+b");assert(image);
 assert(!fseek(image,0,SEEK_END));long size=ftell(image);assert(size>0&&size%512==0);
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="two-button shell test",.architecture="host test, not BSP",
 .block={.sectors=(u32)(size/512),.sector_bytes=512,.writable=1,.read=readsector,.write=writesector,.flush=flush},
 .console={.putc=put,.getc=key,.osk=paint},.stop=stop};
 assert(multi_hal_bind(&h)==0);shell();assert(offset==sizeof(script)-1);assert(!vol.poisoned);
 fclose(image);
 assert(!strstr(captured,"\nSHOULD_NOT_RUN\n"));
 assert(strstr(captured,"input lost")&&strstr(captured,"not executed")&&strstr(captured,"\nRECOVERED\n"));
 puts("PASS actual shell: damaged command not executed, next complete command executes normally");
}
