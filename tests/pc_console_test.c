/* Tests actual VGA driver without privileged I/O or physical memory. */
#define MULTIOS_PC_IO_TEST
#include "multios/hal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drivers/pc-bios/text.c"
u32 multi_test_io_read(u16 p,u8 w){(void)p;(void)w;return 32;}
void multi_test_io_write(u16 p,u32 v,u8 w){(void)p;(void)v;(void)w;}
static int osk_on;
int multi_osk_enabled(void){return osk_on;}
int multi_osk_columns(int r){return r==4?8:12;}
const char *multi_osk_label(int r,int c,int s){(void)r;(void)c;(void)s;return "X";}
void multi_osk_position(int*r,int*c,int*s){*r=*c=*s=0;}
void putch(char c){pc_putc(0,c);}
int main(void){
 u16 guarded[2002];guarded[0]=guarded[2001]=0xdead;vga=guarded+1;
 for(int start=18;start<25;start++){
  osk_on=0;pc_clear(0);for(int i=0;i<start;i++)pc_putc(0,'\n');
  const char *s="MultiOS:/> echo kept";for(int i=0;s[i];i++)pc_putc(0,s[i]);
  for(int n=0;n<3;n++)pc_putc(0,'\b');
  int old_col=col;osk_on=1;pc_osk(0,1,0,0,0);
  assert(row==17&&col==old_col);
  for(int i=0;s[i];i++)assert((vga[17*80+i]&255)==(unsigned char)s[i]);
  for(int i=0;i<3000;i++)pc_putc(0,'Z');
  assert(row<18&&col<80&&guarded[0]==0xdead&&guarded[2001]==0xdead);
 }
 osk_on=0;pc_clear(0);pc_putc(0,(char)0xc2);pc_putc(0,'A');assert((vga[0]&255)=='A');
 puts("PASS actual PC VGA driver: OSK preserves prompt and cursor at rows 18..24; scrolling bounds and malformed UTF-8 recovery");
}
