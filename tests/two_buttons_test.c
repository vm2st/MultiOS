/* vm2 studios | t.me/v2vproj — no hardware dependencies in two-button logic. */
#include "multios/hal.h"
#include "multios/buttons.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;
static int paints;
static int rd(void *c,u32 l,u8 *p){(void)c;(void)l;(void)p;return IO;}
static int get(void *c){(void)c;return -1;}
static void put(void *c,char v){(void)c;(void)v;}
static void paint(void *c,int on,int r,int col,int sh){
 (void)c;(void)on;assert(r>=0&&r<5);assert(col>=0&&col<multi_osk_columns(r));assert(sh==0||sh==1);paints++;
}
static void choose(int tr,int tc){
 for(int i=0;i<56;i++){
  int r,c;multi_osk_position(&r,&c,0);if(r==tr&&c==tc)return;
  assert(multi_osk_process(MULTI_KEY_NEXT)==-2);
 }
 assert(!"unreachable cell using NEXT only");
}
static int select_cell(int r,int c){choose(r,c);return multi_osk_process(MULTI_KEY_SELECT);}
int main(void){
 MultiHal h={.abi=MULTIOS_HAL_ABI,.name="two buttons",.architecture="test",
  .block={.sectors=1,.sector_bytes=512,.read=rd},.console={.putc=put,.getc=get,.osk=paint}};
 assert(multi_hal_bind(&h)==0);
 assert(multi_osk_process(MULTI_KEY_SELECT)==-2&&multi_osk_enabled());
 int r,c,s;multi_osk_position(&r,&c,&s);assert(r==0&&c==0&&s==0);
 int seen[5][12]={{0}};
 for(int i=0;i<56;i++){
  multi_osk_position(&r,&c,0);assert(!seen[r][c]);seen[r][c]=1;
  assert(multi_osk_process(MULTI_KEY_NEXT)==-2);
 }
 multi_osk_position(&r,&c,0);assert(r==0&&c==0);
 for(int i=0;i<56;i++)assert(multi_osk_process(MULTI_KEY_PREVIOUS)==-2);
 multi_osk_position(&r,&c,0);assert(r==0&&c==0);
 assert(multi_osk_process(MULTI_KEY_PREVIOUS)==-2);
 multi_osk_position(&r,&c,0);assert(r==4&&c==7);
 assert(multi_osk_process(MULTI_KEY_NEXT)==-2);
 int ascii[128]={0};
 for(int sh=0;sh<2;sh++){
  for(int rr=0;rr<4;rr++)for(int cc=0;cc<12;cc++){
   int expected=(unsigned char)multi_osk_label(rr,cc,sh)[0];
   assert(select_cell(rr,cc)==expected);ascii[expected]=1;
  }
  if(!sh)assert(select_cell(4,3)==-2);
 }
 assert(select_cell(4,0)==' ');ascii[' ']=1;
 for(int i=32;i<=126;i++)assert(ascii[i]);
 assert(select_cell(4,1)==8);assert(select_cell(4,2)=='\n');
 assert(select_cell(4,5)=='\t');assert(select_cell(4,7)==3);
 assert(select_cell(4,4)==-2&&!multi_osk_enabled());
 assert(multi_osk_process(MULTI_KEY_NEXT)==-2&&multi_osk_enabled());
 multi_osk_position(&r,&c,&s);assert(r==4&&c==4&&s==1);
 assert(select_cell(4,6)==-2&&!multi_osk_enabled());
 assert(multi_osk_process(MULTI_KEY_SELECT)==-2&&multi_osk_enabled());
 assert(multi_osk_process(-1)==-1);
 MultiButtons b={0};
 assert(multi_buttons_update(&b,1)==MULTI_KEY_NEXT);
 for(int i=0;i<100;i++)assert(multi_buttons_update(&b,1)==-2);
 assert(multi_buttons_update(&b,2)==-2);assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_update(&b,2)==MULTI_KEY_SELECT);
 assert(multi_buttons_update(&b,0)==-2);assert(multi_buttons_update(&b,3)==-2);
 assert(multi_buttons_update(&b,1)==-2);assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_update(&b,1)==MULTI_KEY_NEXT);
 assert(multi_buttons_update(0,1)==-2);assert(multi_buttons_update(&b,8)==-2);
 assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_update(&b,4)==-2); /* disabled on two-button BSP */
 assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_init(&b,1)==INVALID);
 assert(multi_buttons_init(&b,3)==0);
 assert(multi_buttons_update(&b,4)==MULTI_KEY_PREVIOUS);
 assert(multi_buttons_update(&b,4)==-2);
 assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_update(&b,5)==-2); /* NEXT+PREV chord */
 assert(multi_buttons_update(&b,4)==-2);
 assert(multi_buttons_update(&b,0)==-2);
 assert(multi_buttons_update(&b,4)==MULTI_KEY_PREVIOUS);
 multi_osk_enable(0);
 assert(multi_osk_process(MULTI_KEY_PREVIOUS)==-2&&multi_osk_enabled());
 multi_osk_enable(0);h.console.osk=0;assert(multi_hal_bind(&h)==0);
 assert(multi_osk_process(MULTI_KEY_SELECT)==-2&&!multi_osk_enabled());
 assert(paints>100);
 puts("Two buttons passed: all 56 cells, all 95 printable ASCII, shift/actions, hide/reopen, wrap, optional third-button PREVIOUS, EOF, no renderer, held/chord/release handling.");
}
