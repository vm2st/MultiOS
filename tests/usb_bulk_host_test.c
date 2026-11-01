/* Real HCD bulk functions, deterministic DMA/register model, NOT hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MULTIOS_PC_IO_TEST 1
#if HOST == 1
#include "drivers/pc-bios/uhci.c"
static Controller c;
#elif HOST == 2
#include "drivers/pc-bios/ohci.c"
static Ohci c;
#else
#include "drivers/pc-bios/ehci.c"
static Ehci c;
#endif
static MultiUsbController info;static u32 regs[128],mode,done,frame,clock_stopped,frame_delay;
void *mzero(void *p,u32 n){return memset(p,0,n);}
static void dma(void){
 if(done)return;
#if HOST == 1
 if(c.control.element==TERMINATE)return;
#elif HOST == 2
 if(c.bulk.flags&SKIP)return;
#else
 if(!(regs[0]&32)||c.ctrl.next!=usb_phys(&c.td[0]))return;
#endif
 if(mode==6)return;
 if(mode==7){
#if HOST == 1
 c.running=0;
#elif HOST == 2
 regs[0x54/4]=0;
#else
 regs[0x44/4]=0;
#endif
 done=1;return;}
 done=1;u32 amount=mode==1?7:mode==2||mode==3||mode==5?0:mode==4?64:512;
 int stall=mode==3||mode==4,err=mode==5;u8 carry=0;
#if HOST == 1
 for(u32 i=0;i<8;i++){
  u32 got=amount>=64?64:amount;amount-=got;
  if(stall||err){if(!got){c.td[i].status=stall?1u<<22:1u<<18;break;}}
  c.td[i].status=(got-1)&2047;carry^=1;
  if(got<64)break;
 }
#elif HOST == 2
 c.td[0].cbp=amount==512?0:usb_phys(c.b.data)+amount;
 c.td[0].flags=(u32)(stall?4:err?5:0)<<28;
 carry=(amount/64+((amount%64)||(!stall&&!err&&amount<512)))&1;
 c.bulk.head=(stall||err?usb_phys(&c.td[0])|1:c.bulk.tail)|(carry<<1);
#else
 carry=(amount/512+((amount%512)||(!stall&&!err&&amount<512)))&1;
 c.td[0].token=((512-amount)<<16)|((u32)carry<<31)|(stall?64:err?8:0);
#endif
}
int pc_delay_ms(u32 ms){(void)ms;
#if HOST == 2
 if(!clock_stopped&&!frame_delay)regs[0x3c/4]++;
#elif HOST == 3
 if(!clock_stopped&&!frame_delay)regs[12/4]+=8;
#endif
 if(frame_delay)frame_delay--;
#if HOST == 3
 regs[1]=(regs[0]&32)?(32u<<10):0;
#endif
 dma();return 0;}
u32 multi_test_io_read(u16 p,u8 w){(void)w;
#if HOST == 1
 dma();if(p==6)return ++frame&2047;if(p==16)return 5;if(p==2)return c.running?0:32;
#endif
 return 0;}
void multi_test_io_write(u16 p,u32 v,u8 w){(void)p;(void)v;(void)w;}
int main(void){
 for(mode=0;mode<8;mode++){
  memset(&c,0,sizeof c);memset(&info,0,sizeof info);memset(regs,0,sizeof regs);done=frame=0;
  c.running=1;c.b.host=&c;c.b.info=&info;c.b.bulk_in_packet=HOST==3?512:64;
#if HOST == 1
 c.info=&info;c.control.element=TERMINATE;
 for(u32 i=0;i<512;i++)c.data[i]=(u8)i;
#elif HOST == 2
 c.regs=regs;regs[0x54/4]=3;c.bulk.flags=SKIP;
 for(u32 i=0;i<512;i++)c.b.data[i]=(u8)i;
#else
 c.op=regs;regs[0x44/4]=5;
 for(u32 i=0;i<512;i++)c.b.data[i]=(u8)i;
#endif
 u8 out[512]={0},toggle=0;u32 actual=999;int r=bulk(&c.b,0x81,out,512,&actual,&toggle);
 if(mode<3){u32 want=mode==0?512:mode==1?7:0;assert(!r&&actual==want);for(u32 i=0;i<want;i++)assert(out[i]==(u8)i);assert(toggle==(mode==0?(HOST==3?1:0):1));}
 else if(mode<5){assert(r==MULTI_USB_STALL&&actual==(mode==4?64:0));}
 else assert(r<0&&r!=MULTI_USB_STALL);
 }
#if HOST == 2
 clock_stopped=1;assert(frames_wait(&c,2)<0);clock_stopped=0;
 frame_delay=7;assert(frames_wait(&c,3)==0);
#endif
 printf("PASS HCD %d: full, short, zero, STALL, partial STALL, error, timeout, disconnect; real bulk function, modeled registers/DMA.\n",HOST);
}
