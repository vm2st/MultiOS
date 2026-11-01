/* MultiOS / MultiPulti — host selection and shared boot-HID enumeration. */
#include "platform/pc-bios/usb.h"
#include "platform/pc-bios/usb-host.h"
static u32 slots;static int initialized;
MultiUsbController *pc_usb_claim(const char *kind,u32 base){
 if(slots>=MULTI_USB_MAX)return 0;
 MultiUsbController *i=multi_hardware_usb(slots++);i->base=base;i->kind=kind;i->status="initializing";return i;
}
int pc_usb_mmio(const MultiPciDevice *d,u32 *base){
 u32 bar=pc_pci_read(d,0x10);
 /* Only identity-mapped 32-bit MMIO. Reject I/O, reserved/64-bit BARs. */
 if(bar&7u || !(bar&~15u) || bar==0xffffffffu)return UNSUP;
 *base=bar&~15u;
 pc_pci_write16(d,4,(pc_pci_read(d,4)&65535)|6);return 0;
}
void multi_usb_init(void){
 if(initialized)return;initialized=1;
 pc_pci_scan();slots=0;pc_usb_mass_reset();
 /* Route low/full-speed ports to companions before they enumerate address 0. */
 pc_ehci_init();pc_ohci_init();pc_uhci_init();
}
int multi_usb_key(void){int k=pc_uhci_key();if(k!=-2)return k;k=pc_ohci_key();return k!=-2?k:pc_ehci_key();}
void multi_usb_stop(void){initialized=0;pc_uhci_stop();pc_ohci_stop();pc_ehci_stop();}
int pc_usb_enumerate(UsbBoot *b){
 b->address=0;b->mps=b->speed==2?64:8;b->configured=0;b->toggle=0;
 multi_hid_reset(&b->hid);mzero(b->data,sizeof(b->data));
 b->info->status="device descriptor";TRY(b->control(b,0x80,6,0x100,0,8));
 if(b->data[0]!=18||b->data[1]!=1)return BAD;
 u8 mps=b->data[7];if(mps!=8&&mps!=16&&mps!=32&&mps!=64)return BAD;
 if((b->speed==1&&mps!=8)||(b->speed==2&&mps!=64))return BAD;b->mps=mps;
 TRY(b->control(b,0,5,1,0,0));b->address=1;TRY(pc_delay_ms(3));
 TRY(b->control(b,0x80,6,0x200,0,9));
 if(b->data[0]!=9||b->data[1]!=2)return BAD;
 u32 len=b->data[2]|((u32)b->data[3]<<8);u8 config=b->data[5];
 if(len<9||len>sizeof(b->data)||!config)return UNSUP;
 TRY(b->control(b,0x80,6,0x200,0,len));
 if(b->data[0]!=9||b->data[1]!=2||b->data[2]!=(len&255)||b->data[3]!=(len>>8)||b->data[5]!=config)return BAD;
 /* Mass storage is a separate device class, not a HID endpoint. */
 int mass_iface=-1,mass=0;u8 bin=0,bout=0;u16 pin=0,pout=0;
 for(u32 pos=0;pos<len;){
  if(len-pos<2)return BAD;u8 n=b->data[pos],type=b->data[pos+1];if(n<2||n>len-pos)return BAD;
  if(type==4){
   if(n<9)return BAD;
   if(mass&&bin&&bout)break;
   mass=b->data[pos+3]==0&&b->data[pos+5]==8&&b->data[pos+6]==6&&b->data[pos+7]==0x50;
   if(mass){mass_iface=b->data[pos+2];bin=bout=0;pin=pout=0;}
  }
  if(type==5&&mass){
   if(n<7)return BAD;u8 e=b->data[pos+2];u16 packet=b->data[pos+4]|((u16)b->data[pos+5]<<8);
   if((b->data[pos+3]&3)==2&&!(e&0x70)&&(e&15)){
    if((b->speed==2&&packet!=512)||(b->speed==0&&packet!=8&&packet!=16&&packet!=32&&packet!=64)||b->speed==1)return UNSUP;
    if(e&128){if(bin)return BAD;bin=e;pin=packet;}else{if(bout)return BAD;bout=e;pout=packet;}
   }
  }pos+=n;
 }
 if(mass_iface>=0&&bin&&bout){
  if(!b->bulk||!b->connected)return UNSUP;
  TRY(b->control(b,0,9,config,0,0));
  return pc_usb_mass_attach(b,mass_iface,bin,bout,pin,pout);
 }
 int interface=-1,eligible=0;u8 ep=0,interval=0;
 for(u32 pos=0;pos<len;){
  if(len-pos<2)return BAD;u8 n=b->data[pos],type=b->data[pos+1];if(n<2||n>len-pos)return BAD;
  if(type==4){if(n<9)return BAD;eligible=b->data[pos+3]==0&&b->data[pos+5]==3&&b->data[pos+6]==1&&b->data[pos+7]==1;if(eligible)interface=b->data[pos+2];}
  if(type==5&&eligible){
   if(n<7)return BAD;u8 e=b->data[pos+2];u16 size=b->data[pos+4]|((u16)b->data[pos+5]<<8);
   if((e&128)&&(e&15)&&!(e&0x70)&&(b->data[pos+3]&3)==3&&size==8){ep=e&15;interval=b->data[pos+6];break;}
  }pos+=n;
 }
 if(interface<0||!ep||!interval||(b->speed==2&&interval>14))return UNSUP;
 TRY(b->control(b,0,9,config,0,0));TRY(b->control(b,0x21,11,0,interface,0));
 /* SET_IDLE is optional; a STALL must not reject an otherwise valid keyboard. */
 (void)b->control(b,0x21,10,0,interface,0);
 b->endpoint=ep;b->interval=interval;b->toggle=0;mzero(b->report,8);
 TRY(b->arm(b));b->configured=1;b->info->keyboards=1;b->info->status="boot-keyboard active";return 0;
}
