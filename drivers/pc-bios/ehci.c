/* MultiOS / MultiPulti — EHCI USB 2.0 high-speed host, polling.
 * Root-port HS boot keyboard; FS/LS delegated to companion UHCI/OHCI.
 * Bulk BOT disks or Boot HID, one direct device/controller. No external hubs, split, ISO or xHCI. */
#include "platform/pc-bios/usb-host.h"
#define END 1u
#define ACTIVE 128u
#define ERR 0x7cu
typedef struct {volatile u32 next,alt,token,buffer[5];} Qtd;
typedef struct {volatile u32 link,endpoint,caps,current,next,alt,token,buffer[5];u32 pad[4];} Qh;
typedef struct {
 UsbBoot b;volatile u32 *caps,*op;u8 running;u16 last;u32 elapsed,scan;
 volatile u32 frames[1024] __attribute__((aligned(4096)));
 Qh ctrl,key __attribute__((aligned(32)));
 Qtd td[3],interrupt __attribute__((aligned(32)));
} Ehci;
_Static_assert(sizeof(Qtd)==32 && sizeof(Qh)==64,"EHCI DMA descriptor size");
static Ehci hosts[MULTIOS_USB_HOST_LIMIT];static u32 count;
static u32 rd(Ehci *c,u32 off){return c->op[off/4];}
static void wr(Ehci *c,u32 off,u32 v){c->op[off/4]=v;usb_barrier();}
static int waitreg(Ehci *c,u32 off,u32 mask,u32 value,u32 ms){
 for(u32 n=0;n<ms;n++){if((rd(c,off)&mask)==value)return 0;TRY(pc_delay_ms(1));}return IO;
}
static int schedule(Ehci *c,u32 bit,int enabled){
 u32 cmd=rd(c,0);if(enabled)cmd|=bit;else cmd&=~bit;wr(c,0,cmd);
 return waitreg(c,4,bit<<10,enabled?bit<<10:0,100);
}
static u32 tick(Ehci *c){u16 f=(rd(c,12)>>3)&1023;c->elapsed+=(f-c->last)&1023;c->last=f;return c->elapsed;}
static void stop(Ehci *c){
 if(!c->running)return;wr(c,0,rd(c,0)&~0x31u);(void)waitreg(c,4,1u<<12,1u<<12,100);
 c->running=0;c->b.configured=0;c->b.info->keyboards=0;c->b.info->status="stopped";
}
void pc_ehci_stop(void){for(u32 i=0;i<count;i++)stop(&hosts[i]);}
static int legacy(Ehci *c,const MultiPciDevice *d){
 u32 ptr=(c->caps[2]>>8)&255;
 for(u32 n=0;ptr&&n<48;n++){
  if(ptr<0x40||ptr>0xf8||(ptr&3))return BAD;u32 v=pc_pci_read(d,ptr);
  if((v&255)==1){
   pc_pci_write16(d,ptr+2,(v>>16)|0x100);
   u32 ms;for(ms=0;ms<500;ms++){if(!(pc_pci_read(d,ptr)&(1u<<16)))break;TRY(pc_delay_ms(1));}
   if(ms==500)return IO;
   pc_pci_write16(d,ptr+4,0);pc_pci_write16(d,ptr+6,0xffff);return 0;
  }
  u32 next=(v>>8)&255;if(next==ptr)return BAD;ptr=next;
 }return ptr?BAD:0;
}
static void qtd(Qtd *q,u8 pid,u8 toggle,u8 *buffer,u32 length,u32 next){
 mzero(q,sizeof(*q));q->next=next;q->alt=END;
 q->token=ACTIVE|(3u<<10)|((u32)pid<<8)|(length<<16)|((u32)toggle<<31);
 if(length){q->buffer[0]=usb_phys(buffer);for(u32 j=1;j<5;j++)q->buffer[j]=(usb_phys(buffer)&~4095u)+j*4096;}
}
static void qh(Qh *q,UsbBoot *b,u8 endpoint,u32 packet){
 mzero(q,sizeof(*q));q->link=END;q->endpoint=b->address|((u32)endpoint<<8)|(2u<<12)|(1u<<14)|(packet<<16);q->caps=1u<<30;q->next=END;q->alt=END;
}
static int control(UsbBoot *b,u8 type,u8 request,u16 value,u16 index,u16 length){
 Ehci *c=b->host;if(!c->running||length>512)return INVALID;
 if(schedule(c,32,0)<0){stop(c);return IO;}
 b->setup[0]=type;b->setup[1]=request;b->setup[2]=value;b->setup[3]=value>>8;b->setup[4]=index;b->setup[5]=index>>8;b->setup[6]=length;b->setup[7]=length>>8;
 u32 n=length?3:2;
 qtd(&c->td[0],2,0,b->setup,8,usb_phys(&c->td[1]));
 if(length)qtd(&c->td[1],type&128?1:0,1,b->data,length,usb_phys(&c->td[2]));
 qtd(&c->td[n-1],type&128?0:1,1,0,0,END);
 qh(&c->ctrl,b,0,b->mps);c->ctrl.endpoint|=1u<<15;c->ctrl.link=usb_phys(&c->ctrl)|2;c->ctrl.next=usb_phys(&c->td[0]);
 usb_barrier();wr(c,0x18,usb_phys(&c->ctrl));
 if(schedule(c,32,1)<0){stop(c);return IO;}
 int result=IO;
 for(u32 ms=0;ms<500;ms++){
  usb_barrier();if(rd(c,4)&(1u<<4))break;
  int active=0,failed=0;for(u32 j=0;j<n;j++){u32 t=c->td[j].token;if(t&ERR)failed=1;if(t&ACTIVE)active=1;}
  if(failed)break;if(!active){result=0;break;}if(pc_delay_ms(1)<0)break;
 }
 if(schedule(c,32,0)<0){stop(c);return IO;}
 if(!result)for(u32 j=0;j<n;j++)if((c->td[j].token>>16)&0x7fff){result=BAD;break;}
 if(result<0)b->info->errors++;wr(c,4,0x3f);return result;
}
static int connected(UsbBoot *b){Ehci *c=b->host;return c->running&&(rd(c,0x44+4*b->port)&5)==5;}
static int bulk(UsbBoot *b,u8 ep,u8 *data,u32 length,u32 *actual,u8 *toggle){
 Ehci *c=b->host;if(!actual||!toggle||!length||length>512||!connected(b))return IO;
 *actual=0;if(schedule(c,32,0)<0){stop(c);return IO;}
 if(!(ep&128))for(u32 i=0;i<length;i++)b->data[i]=data[i];
 qtd(&c->td[0],ep&128?1:0,*toggle,b->data,length,END);
 qh(&c->ctrl,b,ep&15,ep&128?b->bulk_in_packet:b->bulk_out_packet);
 c->ctrl.endpoint|=1u<<15;c->ctrl.link=usb_phys(&c->ctrl)|2;c->ctrl.next=usb_phys(&c->td[0]);
 usb_barrier();wr(c,0x18,usb_phys(&c->ctrl));if(schedule(c,32,1)<0){stop(c);return IO;}
 int result=IO;
 for(u32 ms=0;ms<2000;ms++){
  usb_barrier();u32 t=c->td[0].token;
  if((rd(c,4)&(1u<<4))||!connected(b))break;
  if(t&ERR){result=(t&ERR)==64?MULTI_USB_STALL:IO;break;}
  if(!(t&ACTIVE)){result=0;break;}if(pc_delay_ms(1)<0)break;
 }
 if(schedule(c,32,0)<0){stop(c);return IO;}
 if(result<0&&result!=MULTI_USB_STALL){b->info->errors++;return result;}
 u32 remain=(c->td[0].token>>16)&0x7fff;if(remain>length)return BAD;*actual=length-remain;
 *toggle=(c->td[0].token>>31)&1;
 if(ep&128)for(u32 i=0;i<*actual;i++)data[i]=b->data[i];wr(c,4,0x3f);if(result<0)b->info->errors++;return result;
}
static int arm(UsbBoot *b){
 Ehci *c=b->host;if(!c->running)return IO;
 if(schedule(c,16,0)<0){stop(c);return IO;}
 qh(&c->key,b,b->endpoint,8);c->key.caps|=1; /* one HS poll, microframe 0 */
 qtd(&c->interrupt,1,b->toggle,b->report,8,END);c->key.next=usb_phys(&c->interrupt);
 u32 step=b->interval<=4?1:1u<<(b->interval-4);
 for(u32 j=0;j<1024;j++)c->frames[j]=j%step?END:usb_phys(&c->key)|2;
 usb_barrier();if(schedule(c,16,1)<0){stop(c);return IO;}return 0;
}
static void owner(Ehci *c,u32 p){u32 off=0x44+p*4;wr(c,off,(rd(c,off)&~0x2au)|(1u<<13));}
static int port_setup(Ehci *c,u32 p){
 u32 off=0x44+p*4,v=rd(c,off);if(!(v&1)||(v&(1u<<13)))return NOTFOUND;
 if((v&0xc00)==0x400){owner(c,p);return NOTFOUND;}
 wr(c,off,(v&~0x2eu)|0x100);TRY(pc_delay_ms(55));wr(c,off,(rd(c,off)&~0x12au));
 TRY(waitreg(c,off,0x100,0,100));TRY(pc_delay_ms(10));v=rd(c,off);
 if(!(v&1))return NOTFOUND;if(!(v&4)){owner(c,p);return NOTFOUND;}
 c->b.port=p;c->b.speed=2;return pc_usb_enumerate(&c->b);
}
static void discover(Ehci *c){
 for(u32 p=0;p<c->b.info->ports;p++){
  int e=port_setup(c,p);if(!e)return;
  if(e!=NOTFOUND){u32 off=0x44+p*4;wr(c,off,rd(c,off)&~0x2eu);c->b.info->status=e==UNSUP?"unsupported HS root device":"HS enumeration failed";}
 }
 if(streq(c->b.info->status,"initializing"))c->b.info->status="idle / FS-LS delegated to companions";
}
void pc_ehci_init(void){
 count=0;
 for(u32 i=0;i<multi_hardware_pci_count()&&count<MULTIOS_USB_HOST_LIMIT;i++){
  const MultiPciDevice *d=multi_hardware_pci(i);if(d->base_class!=12||d->subclass!=3||d->interface!=0x20)continue;
  u32 base;if(pc_usb_mmio(d,&base)<0)continue;
  Ehci *c=&hosts[count++];mzero(c,sizeof(*c));c->caps=(volatile u32 *)(uintptr_t)base;
  UsbBoot *b=&c->b;b->host=c;b->control=control;b->arm=arm;b->bulk=bulk;b->connected=connected;b->info=pc_usb_claim("EHCI",base);if(!b->info)break;
  u32 cap=c->caps[0],len=cap&255;if(len<0x10||len>0x80||(len&3)||(cap>>16)!=0x100){b->info->status="unsupported EHCI capability";continue;}
  c->op=(volatile u32 *)(uintptr_t)(base+len);
  if(legacy(c,d)<0){b->info->status="firmware ownership timeout";continue;}
  wr(c,8,0);wr(c,0,0);
  if(waitreg(c,4,1u<<12,1u<<12,100)<0){b->info->status="halt timeout";continue;}
  wr(c,0,2);if(waitreg(c,0,2,0,100)<0){b->info->status="reset timeout";continue;}
  for(u32 j=0;j<1024;j++)c->frames[j]=END;
  wr(c,0x10,0);wr(c,0x14,usb_phys(c->frames));wr(c,0x18,0);wr(c,12,0);wr(c,4,0x3f);
  wr(c,0,1);c->running=1;
  if(waitreg(c,4,1u<<12,0,100)<0){stop(c);continue;}
  wr(c,0x40,1);b->info->ports=c->caps[1]&15;
  if(!b->info->ports){stop(c);b->info->status="invalid root hub";continue;}
  for(u32 p=0;p<b->info->ports;p++){u32 off=0x44+p*4;wr(c,off,(rd(c,off)&~0x2au)|(1u<<12));}
  if(pc_delay_ms(30)<0){stop(c);continue;}discover(c);c->scan=tick(c);
 }
}
int pc_ehci_key(void){
 for(u32 i=0;i<count;i++){
  Ehci *c=&hosts[i];UsbBoot *b=&c->b;if(!c->running)continue;u32 now=tick(c);
  if(rd(c,4)&((1u<<4)|(1u<<12))){b->info->errors++;stop(c);b->info->status="EHCI host system error";continue;}
  if(b->configured==2){pc_usb_mass_poll(b);continue;}
  if(b->configured){
   if((rd(c,0x44+4*b->port)&5)!=5){
    if(schedule(c,16,0)<0){stop(c);continue;}b->configured=0;b->info->keyboards=0;b->info->disconnects++;multi_hid_reset(&b->hid);b->info->status="disconnected";
   }else {
    usb_barrier();u32 t=c->interrupt.token;
    if(!(t&ACTIVE)){
     if(schedule(c,16,0)<0){stop(c);continue;}
     if((t&ERR)||((t>>16)&0x7fff)){b->info->errors++;b->configured=0;b->info->keyboards=0;b->info->status="interrupt transfer error";}
     else {b->info->reports++;multi_hid_report(&b->hid,b->report,8);b->toggle^=1;if(arm(b)<0)continue;}
    }
    int k=multi_hid_get(&b->hid);if(k!=-2)return k;
   }
  }
  /* Port-change is the primary hotplug trigger. FRINDEX may stop advancing
   * in virtual hosts with both schedules idle; do not require a time tick. */
  int changed=0;
  for(u32 p=0;p<b->info->ports;p++){
   u32 off=0x44+p*4,v=rd(c,off);
   if(v&0x2a){wr(c,off,(v&~0x2au)|0x2a);if((v&1)&&!(v&(1u<<13)))changed=1;}
  }
  if(!b->configured&&(changed||now-c->scan>=500)){c->scan=now;discover(c);}
 }return -2;
}
