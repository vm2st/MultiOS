/* MultiOS / MultiPulti — OHCI 1.0/1.1, polled, coherent identity-mapped DMA.
 * One direct root-port Boot HID or BOT disk per controller. No hubs/ISO. */
#include "platform/pc-bios/usb-host.h"
#define SKIP (1u<<14)
typedef struct {volatile u32 flags,tail,head,next;} Ed;
typedef struct {volatile u32 flags,cbp,next,end;} Td;
typedef struct {volatile u32 interrupt[32];volatile u16 frame,pad;volatile u32 done;u8 reserved[120];} Hcca;
typedef struct {
 UsbBoot b;volatile u32 *regs;u8 running;u16 last;u32 elapsed,scan;
 Hcca hcca __attribute__((aligned(256)));
 Ed ctrl,key,bulk __attribute__((aligned(16)));
 Td td[67],keys[2] __attribute__((aligned(16)));
} Ohci;
_Static_assert(sizeof(Hcca)==256,"OHCI HCCA size");
_Static_assert(sizeof(Ed)==16 && sizeof(Td)==16,"OHCI DMA descriptor size");
static Ohci hosts[MULTIOS_USB_HOST_LIMIT];static u32 count;
static u32 rd(Ohci *c,u32 off){return c->regs[off/4];}
static void wr(Ohci *c,u32 off,u32 v){c->regs[off/4]=v;usb_barrier();}
static int waitreg(Ohci *c,u32 off,u32 mask,u32 value,u32 ms){
 for(u32 n=0;n<ms;n++){if((rd(c,off)&mask)==value)return 0;TRY(pc_delay_ms(1));}return IO;
}
static u32 tick(Ohci *c){u16 f=rd(c,0x3c);c->elapsed+=(u16)(f-c->last);c->last=f;return c->elapsed;}
static void stop(Ohci *c){
 if(!c->running)return;wr(c,4,0);wr(c,8,1);(void)waitreg(c,8,1,0,20);
 c->running=0;c->b.configured=0;c->b.info->keyboards=0;c->b.info->status="stopped";
}
void pc_ohci_stop(void){for(u32 i=0;i<count;i++)stop(&hosts[i]);}
/* Wait for observed USB frames, not an assumed PIT-to-USB timer phase.
 * A stopped controller still fails closed; startup may settle for <=100 polls. */
static int frames_wait(Ohci *c,u32 frames){
 u16 start=rd(c,0x3c);
 for(u32 n=0;n<100;n++){if((u16)((u16)rd(c,0x3c)-start)>=frames)return 0;TRY(pc_delay_ms(1));}return IO;
}
static int quiesce(Ohci *c){if(frames_wait(c,2)<0){stop(c);return IO;}return 0;}
static void td(Td *t,u8 direction,u8 toggle,u8 *data,u32 length,u32 next){
 t->flags=(15u<<28)|(7u<<21)|((u32)direction<<19)|((u32)(2+toggle)<<24);
 t->cbp=length?usb_phys(data):0;t->end=length?usb_phys(data+length-1):0;t->next=next;
}
static int control(UsbBoot *b,u8 type,u8 request,u16 value,u16 index,u16 length){
 Ohci *c=b->host;if(!c->running||length>512)return INVALID;
 b->setup[0]=type;b->setup[1]=request;b->setup[2]=value;b->setup[3]=value>>8;b->setup[4]=index;b->setup[5]=index>>8;b->setup[6]=length;b->setup[7]=length>>8;
 u32 n=(length+b->mps-1)/b->mps+2;if(n>66)return UNSUP;
 c->ctrl.flags=SKIP;usb_barrier();TRY(quiesce(c));
 td(&c->td[0],0,0,b->setup,8,usb_phys(&c->td[1]));
 u32 off=0;u8 toggle=1;
 for(u32 i=1;i<n-1;i++){u32 len=length-off;if(len>b->mps)len=b->mps;td(&c->td[i],type&128?2:1,toggle,b->data+off,len,usb_phys(&c->td[i+1]));toggle^=1;off+=len;}
 td(&c->td[n-1],type&128?1:2,1,0,0,usb_phys(&c->td[n]));mzero(&c->td[n],sizeof(Td));
 c->ctrl.tail=usb_phys(&c->td[n]);c->ctrl.head=usb_phys(&c->td[0]);c->ctrl.next=0;
 c->ctrl.flags=b->address|((u32)b->mps<<16)|(b->speed==1?(1u<<13):0);usb_barrier();wr(c,8,2);
 int result=IO;
 for(u32 ms=0;ms<500;ms++){
  usb_barrier();if(rd(c,12)&16)break;
  if(c->ctrl.head&1)break;
  if((c->ctrl.head&~15u)==c->ctrl.tail){result=0;break;}
  if(pc_delay_ms(1)<0)break;
 }
 c->ctrl.flags|=SKIP;usb_barrier();if(quiesce(c)<0)return IO;
 if(result==0)for(u32 i=0;i<n;i++)if((c->td[i].flags>>28)||c->td[i].cbp){result=IO;break;}
 if(result<0)b->info->errors++;
 return result;
}
static int connected(UsbBoot *b){Ohci *c=b->host;return c->running&&(rd(c,0x54+4*b->port)&3)==3;}
static int bulk(UsbBoot *b,u8 ep,u8 *data,u32 length,u32 *actual,u8 *toggle){
 Ohci *c=b->host;if(!actual||!toggle||!length||length>512||!connected(b))return IO;*actual=0;
 u32 packet=ep&128?b->bulk_in_packet:b->bulk_out_packet;
 if(!packet||packet>64)return UNSUP;
 c->bulk.flags|=SKIP;usb_barrier();TRY(quiesce(c));
 if(!(ep&128))for(u32 i=0;i<length;i++)b->data[i]=data[i];
 /* One general TD lets OHCI stop at a short packet without issuing another
  * data TD that could swallow a BOT CSW. Rounding is valid for IN only. */
 td(&c->td[0],ep&128?2:1,*toggle,b->data,length,usb_phys(&c->td[1]));
 if(ep&128)c->td[0].flags|=1u<<18;
 mzero(&c->td[1],sizeof(Td));
 c->bulk.tail=usb_phys(&c->td[1]);c->bulk.head=usb_phys(&c->td[0]);c->bulk.next=0;
 c->bulk.flags=b->address|((u32)(ep&15)<<7)|(packet<<16);
 usb_barrier();wr(c,8,4);int result=IO;
 for(u32 ms=0;ms<2000;ms++){
  usb_barrier();if((rd(c,12)&16)||!connected(b))break;
  u32 cc=c->td[0].flags>>28;
  if(cc<14){result=cc==0?0:cc==4?MULTI_USB_STALL:IO;break;}
  if(c->bulk.head&1)break;if(pc_delay_ms(1)<0)break;
 }
 c->bulk.flags|=SKIP;usb_barrier();if(quiesce(c)<0)return IO;
 u32 cbp=c->td[0].cbp,base=usb_phys(b->data);
 if(cbp&&(cbp<base||cbp>base+length)){b->info->errors++;return IO;}
 *actual=cbp?cbp-base:length;*toggle=(c->bulk.head>>1)&1;
 if(ep&128)for(u32 i=0;i<*actual;i++)data[i]=b->data[i];
 if(result<0)b->info->errors++;return result;
}
static int arm(UsbBoot *b){
 Ohci *c=b->host;if(!c->running)return IO;if(b->interval>32)return UNSUP;
 c->key.flags|=SKIP;usb_barrier();TRY(quiesce(c));
 mzero(c->keys,sizeof(c->keys));td(&c->keys[0],2,b->toggle,b->report,8,usb_phys(&c->keys[1]));
 u32 step=1;while(step<b->interval)step<<=1;
 for(u32 j=0;j<32;j++)c->hcca.interrupt[j]=j%step?0:usb_phys(&c->key);
 c->key.tail=usb_phys(&c->keys[1]);c->key.head=usb_phys(&c->keys[0]);c->key.next=0;
 c->key.flags=b->address|((u32)b->endpoint<<7)|(2u<<11)|(b->speed==1?(1u<<13):0)|(8u<<16);
 usb_barrier();return 0;
}
static void port_disable(Ohci *c,u32 p){wr(c,0x54+4*p,1);}
static int port_setup(Ohci *c,u32 p){
 u32 reg=0x54+p*4;if(!(rd(c,reg)&1))return NOTFOUND;
 wr(c,reg,1u<<8);TRY(pc_delay_ms(20));wr(c,reg,1u<<4);
 TRY(waitreg(c,reg,1u<<4,0,100));if((rd(c,reg)&3)!=3)return IO;
 wr(c,reg,0x1f0000);TRY(pc_delay_ms(10));
 c->b.port=p;c->b.speed=(rd(c,reg)&(1u<<9))?1:0;
 return pc_usb_enumerate(&c->b);
}
static void discover(Ohci *c){
 for(u32 p=0;p<c->b.info->ports;p++){
  int e=port_setup(c,p);if(!e)return;port_disable(c,p);
  if(e!=NOTFOUND)c->b.info->status=e==UNSUP?"unsupported root device":"enumeration failed";
 }
 if(!c->b.info->status||streq(c->b.info->status,"initializing"))c->b.info->status="idle: no direct keyboard";
}
void pc_ohci_init(void){
 count=0;
 for(u32 i=0;i<multi_hardware_pci_count()&&count<MULTIOS_USB_HOST_LIMIT;i++){
  const MultiPciDevice *d=multi_hardware_pci(i);if(d->base_class!=12||d->subclass!=3||d->interface!=0x10)continue;
  u32 base;if(pc_usb_mmio(d,&base)<0)continue;
  Ohci *c=&hosts[count++];mzero(c,sizeof(*c));c->regs=(volatile u32 *)(uintptr_t)base;
  UsbBoot *b=&c->b;b->host=c;b->control=control;b->arm=arm;b->bulk=bulk;b->connected=connected;b->info=pc_usb_claim("OHCI",base);if(!b->info)break;
  if((rd(c,0)&255)!=0x10){b->info->status="unsupported OHCI revision";continue;}
  if(rd(c,4)&256){wr(c,8,8);if(waitreg(c,4,256,0,500)<0){b->info->status="firmware ownership timeout";continue;}}
  wr(c,0x14,0xffffffffu);wr(c,4,0);wr(c,8,1);
  if(waitreg(c,8,1,0,50)<0){b->info->status="reset timeout";continue;}
  wr(c,0x34,0x27782edf);wr(c,0x40,0x2a2f);wr(c,0x44,0x628);
  c->ctrl.flags=SKIP;c->key.flags=SKIP;c->bulk.flags=SKIP;
  for(u32 j=0;j<32;j++)c->hcca.interrupt[j]=usb_phys(&c->key);
  wr(c,0x18,usb_phys(&c->hcca));wr(c,0x20,usb_phys(&c->ctrl));wr(c,0x24,0);wr(c,0x28,usb_phys(&c->bulk));wr(c,0x2c,0);
  wr(c,0x0c,0xffffffffu);wr(c,4,0xb4);c->running=1;
  b->info->ports=rd(c,0x48)&255;if(!b->info->ports||b->info->ports>15){stop(c);b->info->status="invalid root hub";continue;}
  wr(c,0x50,1u<<16);for(u32 p=0;p<b->info->ports;p++)wr(c,0x54+p*4,1u<<8);
  if(pc_delay_ms(2*((rd(c,0x48)>>24)&255)+20)<0){stop(c);continue;}
  if(frames_wait(c,3)<0){stop(c);b->info->status="frame timer stopped";continue;}
  discover(c);c->scan=tick(c);
 }
}
int pc_ohci_key(void){
 for(u32 i=0;i<count;i++){
  Ohci *c=&hosts[i];UsbBoot *b=&c->b;if(!c->running)continue;u32 now=tick(c);
  if(rd(c,12)&16){b->info->errors++;stop(c);b->info->status="unrecoverable OHCI error";continue;}
  if(b->configured==2){pc_usb_mass_poll(b);continue;}
  if(b->configured){
   if((rd(c,0x54+4*b->port)&3)!=3){c->key.flags|=SKIP;usb_barrier();(void)quiesce(c);b->configured=0;b->info->keyboards=0;b->info->disconnects++;multi_hid_reset(&b->hid);b->info->status="disconnected";}
   else {
    usb_barrier();u32 cc=c->keys[0].flags>>28;
    if(cc<14){
     c->key.flags|=SKIP;usb_barrier();
     if(cc||c->keys[0].cbp){b->info->errors++;b->info->keyboards=0;b->configured=0;b->info->status="interrupt transfer error";port_disable(c,b->port);}
     else {b->info->reports++;multi_hid_report(&b->hid,b->report,8);b->toggle^=1;if(arm(b)<0){stop(c);continue;}}
    }
    int k=multi_hid_get(&b->hid);if(k!=-2)return k;
   }
  }
  if(!b->configured&&now-c->scan>=500){c->scan=now;discover(c);}
 }return -2;
}
