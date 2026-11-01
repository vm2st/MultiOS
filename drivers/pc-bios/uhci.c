/* MultiPulti | vm2 studios | t.me/v2vproj — own polled PCI UHCI HID host.
 * Identity-mapped coherent DMA, i386 BIOS BSP only; no firmware calls.
 * Direct root-port boot keyboard, 1/controller, bounded controller count per profile.
 * Boot HID or BOT disk, one direct device/controller; no external hubs, IRQs or xHCI.
 */
#include "platform/pc-bios/io.h"
#include "platform/pc-bios/pci.h"
#include "platform/pc-bios/usb-host.h"
#include "multios/hid.h"
#define ACTIVE (1u<<23)
#define ERRORS ((1u<<22)|(1u<<21)|(1u<<20)|(1u<<18)|(1u<<17))
#define TERMINATE 1u
#define PID_IN 0x69u
#define PID_OUT 0xe1u
#define PID_SETUP 0x2du
typedef struct {volatile u32 link,status,token,buffer;} Td;
typedef struct {volatile u32 horizontal,element,pad[2];} Qh;
typedef struct {
 UsbBoot b;
 volatile u32 frames[1024] __attribute__((aligned(4096)));
 Qh control,key __attribute__((aligned(16)));
 Td td[66] __attribute__((aligned(16)));
 Td interrupt __attribute__((aligned(16)));
 u8 setup[8],data[512],report[8];
 MultiHid hid;MultiPciDevice pci;MultiUsbController *info;
 u16 io,last_frame;u32 elapsed,last_scan;u8 running,port,speed,address,endpoint,interval,toggle;
} Controller;
static Controller controllers[MULTIOS_USB_HOST_LIMIT];static u32 total;
static void barrier(void){__asm__ volatile("":::"memory");}
static u32 physical(const volatile void *p){return (u32)(uintptr_t)p;}
static u32 ticks(Controller *c){u16 f=iw(c->io+6)&2047;c->elapsed+=(f-c->last_frame)&2047;c->last_frame=f;return c->elapsed;}
static int frames_wait(Controller *c,u32 ms){u32 start=ticks(c),spin=0;while(ticks(c)-start<ms)if(++spin>4000000)return IO;return 0;}
static void terminate(Controller *c){
 if(!c->running)return;ow(c->io,0);for(u32 i=0;i<100000;i++)if(iw(c->io+2)&32)break;
 c->running=0;c->info->keyboards=0;c->info->status="stopped";
}
void pc_uhci_stop(void){for(u32 i=0;i<total;i++)terminate(&controllers[i]);}
static void td_set(Controller *c,Td *t,u32 pid,u8 addr,u8 endpoint,u8 toggle,u8 *data,u32 n,u32 next){
 t->link=next;t->token=pid|((u32)addr<<8)|((u32)endpoint<<15)|((u32)toggle<<19)|(((n-1)&2047)<<21);
 t->buffer=n?physical(data):0;t->status=ACTIVE|(3u<<27)|(c->speed?(1u<<26):0)|2047;barrier();
}
static int control(Controller *c,u8 type,u8 request,u16 value,u16 index,u16 length){
 if(length>512)return INVALID;
 c->setup[0]=type;c->setup[1]=request;c->setup[2]=value;c->setup[3]=value>>8;
 c->setup[4]=index;c->setup[5]=index>>8;c->setup[6]=length;c->setup[7]=length>>8;
 /* Boot endpoint 0 starts at 8 bytes; read device prefix first, then use MPS. */
 u32 packet=c->data[511]?c->data[511]:8;u32 count=(length+packet-1)/packet+2;
 if(count>66)return UNSUP;
 td_set(c,&c->td[0],PID_SETUP,c->address,0,0,c->setup,8,physical(&c->td[1])|4);
 u32 offset=0,toggle=1;
 for(u32 i=1;i<count-1;i++){
  u32 n=length-offset;if(n>packet)n=packet;
  td_set(c,&c->td[i],type&128?PID_IN:PID_OUT,c->address,0,toggle,c->data+offset,n,physical(&c->td[i+1])|4);
  toggle^=1;offset+=n;
 }
 td_set(c,&c->td[count-1],type&128?PID_OUT:PID_IN,c->address,0,1,0,0,TERMINATE);
 barrier();c->control.element=physical(&c->td[0]);barrier();u32 start=ticks(c),spin=0;int result=0;
 for(;;){
  barrier();int pending=0;
  for(u32 i=0;i<count;i++){u32 s=c->td[i].status;if(s&ERRORS){result=IO;break;}if(s&ACTIVE)pending=1;}
  if(result||!pending)break;
  if(ticks(c)-start>500||++spin>8000000){result=IO;break;}
  if(iw(c->io+2)&0x18){result=IO;break;}
 }
 c->control.element=TERMINATE;barrier();if(frames_wait(c,2)<0)result=IO;
 if(result<0){c->info->errors++;return result;}
 /* Reject short/malformed descriptor data; never consume stale DMA bytes. */
 for(u32 i=1;i<count-1;i++){
  u32 actual=(c->td[i].status+1)&2047;u32 expected=((c->td[i].token>>21)+1)&2047;
  if(actual!=expected){c->info->errors++;return BAD;}
 }
 return 0;
}
static void arm(Controller *c){
 td_set(c,&c->interrupt,PID_IN,c->address,c->endpoint,c->toggle,c->report,8,TERMINATE);
 barrier();c->key.element=physical(&c->interrupt);barrier();
}
static void disable_port(Controller *c,u8 p){u16 reg=c->io+16+p*2;u16 v=iw(reg);ow(reg,v&~(4|2|8|512));}
static int connected(UsbBoot *b){Controller *c=b->host;return c->running&&(iw(c->io+16+b->port*2)&5)==5;}
static int usb_control(UsbBoot *b,u8 type,u8 req,u16 value,u16 index,u16 length){
 Controller *c=b->host;c->address=b->address;c->data[511]=b->mps;
 int e=control(c,type,req,value,index,length);if(e>=0&&length)for(u32 i=0;i<length;i++)b->data[i]=c->data[i];return e;
}
static int usb_arm(UsbBoot *b){
 Controller *c=b->host;c->address=b->address;c->port=b->port;c->endpoint=b->endpoint;c->interval=b->interval;c->toggle=b->toggle;
 multi_hid_reset(&c->hid);mzero(c->report,8);arm(c);return 0;
}
static int bulk(UsbBoot *b,u8 ep,u8 *data,u32 length,u32 *actual,u8 *toggle){
 Controller *c=b->host;if(!actual||!toggle||!length||length>512||!connected(b))return IO;*actual=0;
 u32 packet=ep&128?b->bulk_in_packet:b->bulk_out_packet;if(!packet||packet>64)return UNSUP;
 if(!(ep&128))for(u32 i=0;i<length;i++)c->data[i]=data[i];
 u32 n=(length+packet-1)/packet,off=0;u8 next=*toggle;
 for(u32 i=0;i<n;i++){u32 len=length-off;if(len>packet)len=packet;td_set(c,&c->td[i],ep&128?PID_IN:PID_OUT,b->address,ep&15,next,c->data+off,len,i+1<n?physical(&c->td[i+1])|4:TERMINATE);if(ep&128)c->td[i].status|=1u<<29;off+=len;next^=1;}
 barrier();c->control.element=physical(&c->td[0]);barrier();int result=IO;u32 start=ticks(c),spin=0;
 for(;;){
  barrier();int done=1;
  for(u32 i=0;i<n;i++){
   u32 t=c->td[i].status;
   if(t&ERRORS){result=(t&ERRORS)==(1u<<22)?MULTI_USB_STALL:IO;goto quiesce;}
   if(t&ACTIVE){done=0;break;}
   u32 got=(t+1)&2047,want=((c->td[i].token>>21)+1)&2047;
   if(got>want){result=IO;goto quiesce;}
   if(got<want){result=(ep&128)?0:IO;goto quiesce;}
  }
  if(!connected(b)||(iw(c->io+2)&0x38))break;
  if(done){result=0;break;}if(ticks(c)-start>2000||++spin>16000000)break;
 }
quiesce:
 c->control.element=TERMINATE;barrier();if(frames_wait(c,2)<0){terminate(c);return IO;}
 /* Completed prefix only: a short IN must not consume the following CSW. */
 for(u32 i=0;i<n;i++){
  u32 t=c->td[i].status;if(t&(ACTIVE|ERRORS))break;
  u32 got=(t+1)&2047,want=((c->td[i].token>>21)+1)&2047;
  if(got>want||*actual+got>length){result=IO;break;}
  *actual+=got;*toggle^=1;if(got<want)break;
 }
 if(ep&128)for(u32 i=0;i<*actual;i++)data[i]=c->data[i];
 if(result<0)c->info->errors++;return result;
}
static int enumerate(Controller *c,u8 port){
 u16 reg=c->io+16+port*2;u16 v=iw(reg);if(!(v&1))return NOTFOUND;
 c->info->status="enumerating";disable_port(c,port);
 ow(reg,(v&~(2|8|4))|512);if(pc_delay_ms(55)<0)return IO;
 v=iw(reg);ow(reg,v&~(512|2|8));if(pc_delay_ms(12)<0)return IO;
 v=iw(reg);ow(reg,(v&~(512|2|8))|4);if(pc_delay_ms(12)<0)return IO;
 v=iw(reg);if((v&5)!=5)return IO;
 c->speed=!!(v&256);c->port=port;c->b.port=port;c->b.speed=c->speed?1:0;
 return pc_usb_enumerate(&c->b);
}
static void discover(Controller *c){
 int connected=0;
 for(u8 p=0;p<c->info->ports;p++){
  if(iw(c->io+16+p*2)&1)connected=1;
  if(enumerate(c,p)==0)return;
  disable_port(c,p);
 }
 if(!connected)c->info->status="idle: no direct keyboard";
}
void pc_uhci_init(void){
 total=0;
 for(u32 i=0;i<multi_hardware_pci_count()&&total<MULTIOS_USB_HOST_LIMIT;i++){
  const MultiPciDevice *d=multi_hardware_pci(i);
  if(d->base_class!=12||d->subclass!=3||d->interface!=0)continue;
  u32 bar=pc_pci_read(d,0x20);if(!(bar&1)||(bar&0xffff0000u)||!(bar&0xffe0))continue;
  Controller *c=&controllers[total];mzero(c,sizeof(*c));c->pci=*d;c->io=bar&0xffe0;c->info=pc_usb_claim("UHCI",c->io);if(!c->info)break;c->info->io=c->io;
  c->b.host=c;c->b.control=usb_control;c->b.arm=usb_arm;c->b.bulk=bulk;c->b.connected=connected;c->b.info=c->info;
  c->info->status="initializing";
  pc_pci_write16(d,4,(pc_pci_read(d,4)&65535)|5); /* IO + bus master; no status W1C writes. */
  if(d->vendor==0x8086)pc_pci_write16(d,0xc0,0x2000); /* Intel UHCI legacy SMI/PS2 emulation off. */
  ow(c->io,0);ow(c->io+4,0);ow(c->io,2);
  u32 spin=0;while((iw(c->io)&2)&&++spin<100000){}
  if(spin>=100000){c->info->status="reset timeout";total++;continue;}
  c->control.horizontal=physical(&c->key)|2;c->control.element=TERMINATE;
  c->key.horizontal=TERMINATE;c->key.element=TERMINATE;
  for(u32 f=0;f<1024;f++)c->frames[f]=physical(&c->control)|2;
  ow(c->io+2,0x3f);ow(c->io+6,0);ol(c->io+8,physical(c->frames));ob(c->io+12,0x40);barrier();
  for(u32 p=0;p<2;p++){u16 v=iw(c->io+16+p*2);if(v!=0xffff&&(v&128))c->info->ports++;else break;}
  for(u8 p=0;p<c->info->ports;p++)disable_port(c,p);
  ow(c->io,0xc1);c->running=1;total++;
  if(frames_wait(c,3)<0){terminate(c);c->info->status="frame timer stopped";continue;}
  discover(c);c->last_scan=ticks(c);
 }
}
int pc_uhci_key(void){
 for(u32 i=0;i<total;i++){
  Controller *c=&controllers[i];if(!c->running)continue;
  if(c->b.configured==2){pc_usb_mass_poll(&c->b);continue;}
  u32 now=ticks(c);u16 status=iw(c->io+2);
  if(status&0x38){c->info->errors++;terminate(c);c->info->status="controller fault: halted";continue;}
  if(c->info->keyboards){
   u16 port=iw(c->io+16+c->port*2);
   if((port&5)!=5){c->key.element=TERMINATE;barrier();frames_wait(c,2);c->info->keyboards=0;c->info->disconnects++;multi_hid_reset(&c->hid);c->info->status="disconnected";}
   else {
    barrier();u32 s=c->interrupt.status;
    if(!(s&ACTIVE)){
     c->key.element=TERMINATE;barrier();
     if(frames_wait(c,2)<0){terminate(c);c->info->errors++;continue;}
     if(s&ERRORS){c->info->errors++;c->info->keyboards=0;c->info->status="endpoint error";disable_port(c,c->port);}
     else if(((s+1)&2047)!=8){c->info->errors++;c->toggle^=1;arm(c);}
     else {c->info->reports++;multi_hid_report(&c->hid,c->report,8);c->toggle^=1;arm(c);}
    }
    int key=multi_hid_get(&c->hid);if(key!=-2)return key;
   }
  }
  if(!c->info->keyboards&&now-c->last_scan>=500){c->last_scan=now;discover(c);}
 }
 return -2;
}
