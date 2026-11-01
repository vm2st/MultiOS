/* MultiOS — actual shared enumerator, deterministic malformed-device tests. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "platform/pc-bios/usb-host.h"
static int mass_mode,mass_bad,mass_attached;
int pc_usb_mass_attach(UsbBoot *b,u8 i,u8 in,u8 out,u16 pi,u16 po){assert(i==0&&in==0x81&&out==2&&pi==po&&pi==(b->speed==2?512:64));mass_attached++;b->configured=2;return 0;}

static int bad,armed,idle_fails,proto_fails,arm_fails,mps=8,interval=10;
void *mzero(void *p,u32 n){return memset(p,0,n);}
int pc_delay_ms(u32 ms){(void)ms;return 0;}
static int request(UsbBoot *b,u8 t,u8 r,u16 v,u16 i,u16 n){
 (void)t;(void)i;
 if(r==6&&v==0x100){memset(b->data,0,n);b->data[0]=18;b->data[1]=1;b->data[7]=mps;return bad==1?IO:0;}
 if(r==6&&v==0x200){
  if(mass_mode){
   u8 d[32]={9,2,32,0,1,1,0,0x80,50,9,4,0,0,2,8,6,0x50,0,7,5,0x81,2,64,0,0,7,5,2,2,64,0,0};
   if(mps==64){d[22]=d[29]=0;d[23]=d[30]=2;}
   if(mass_bad==1)d[16]=0x62;if(mass_bad==2)d[27]=0x82;if(mass_bad==3)d[22]=63;if(mass_bad==4)d[18]=0;
   memcpy(b->data,d,n);return 0;
  }

  u8 d[34]={9,2,34,0,1,1,0,0x80,50,9,4,0,0,1,3,1,1,0,9,0x21,0x11,1,0,1,0x22,63,0,7,5,0x81,3,8,0,10};
  d[33]=interval;if(bad==2)d[9]=0;if(bad==3)d[16]=2;if(bad==4)d[28]=0x01;
  memcpy(b->data,d,n);return 0;
 }
 if(r==11&&proto_fails)return IO;if(r==10&&idle_fails)return IO;return 0;
}
static int bulk_stub(UsbBoot *b,u8 e,u8 *d,u32 n,u32 *a,u8 *t){(void)b;(void)e;(void)d;(void)n;(void)a;(void)t;return IO;}
static int connected_stub(UsbBoot *b){(void)b;return 1;}
static int arm(UsbBoot *b){(void)b;armed++;return arm_fails?IO:0;}
static int run(int speed){UsbBoot b;MultiUsbController info;memset(&b,0,sizeof(b));memset(&info,0,sizeof(info));b.info=&info;b.control=request;b.arm=arm;b.bulk=bulk_stub;b.connected=connected_stub;b.speed=speed;int e=pc_usb_enumerate(&b);assert((e==0)==(b.configured==(mass_mode?2:1)));if(!e&&!mass_mode){assert(info.keyboards==1);assert(b.endpoint==1);assert(b.address==1);}return e;}
void multi_hid_reset(MultiHid *h){memset(h,0,sizeof(*h));}
int main(void){
 assert(run(0)==0);assert(run(1)==0);mps=64;assert(run(2)==0);
 mps=8;for(bad=1;bad<=4;bad++)assert(run(0)<0);bad=0;
 proto_fails=1;assert(run(0)<0);proto_fails=0;idle_fails=1;assert(run(0)==0);idle_fails=0;
 arm_fails=1;assert(run(0)<0);arm_fails=0;
 mps=9;assert(run(0)==BAD);mps=64;assert(run(1)==BAD);mps=8;assert(run(2)==BAD);
 mps=64;interval=15;assert(run(2)==UNSUP);interval=14;assert(run(2)==0);interval=0;assert(run(2)==UNSUP);
 mass_mode=1;interval=10;mps=8;assert(run(0)==0);mps=64;assert(run(2)==0);mps=8;assert(run(1)==UNSUP);
 for(mass_bad=1;mass_bad<=4;mass_bad++)assert(run(0)<0);assert(mass_attached==2);
 puts("PASS: 23 shared USB enumeration cases, including malformed descriptors, speed/MPS, SET_PROTOCOL, optional SET_IDLE, interrupt interval and arm failures.");return 0;
}
