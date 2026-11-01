/* MultiPulti | vm2 studios | t.me/v2vproj — PCI configuration mechanism #1. */
#include "platform/pc-bios/io.h"
#include "platform/pc-bios/pci.h"
static u32 address(const MultiPciDevice *d,u8 o){return 0x80000000u|((u32)d->bus<<16)|((u32)d->slot<<11)|((u32)d->function<<8)|(o&0xfc);}
u32 pc_pci_read(const MultiPciDevice *d,u8 o){ol(0xcf8,address(d,o));return il(0xcfc);}
void pc_pci_write16(const MultiPciDevice *d,u8 o,u16 v){ol(0xcf8,address(d,o));ow(0xcfc+(o&2),v);}
void pc_pci_scan(void){
 multi_hardware_reset();
 for(u32 b=0;b<256;b++)for(u32 s=0;s<32;s++){
  MultiPciDevice d={.bus=b,.slot=s};u32 id=pc_pci_read(&d,0);
  if(id==0xffffffffu||(id&65535)==0xffff)continue;
  u32 functions=(pc_pci_read(&d,12)&0x800000)?8:1;
  for(u32 f=0;f<functions;f++){
   d.function=f;id=pc_pci_read(&d,0);if((id&65535)==0xffff)continue;
   u32 cl=pc_pci_read(&d,8);d.vendor=id;d.device=id>>16;d.base_class=cl>>24;d.subclass=cl>>16;d.interface=cl>>8;
   multi_hardware_pci_add(&d);
  }
 }
 multi_hardware_pci_finish(1);
}
/* PIT channel 2 one-shot, polled, preserving speaker/gate state. No TSC needed.
 * Failure is bounded; no guessed CPU-frequency delay or interrupt handler. */
int pc_delay_ms(u32 ms){
 while(ms){u32 chunk=ms>50?50:ms;ms-=chunk;u16 ticks=chunk*1193;
  u8 saved=ib(0x61);ob(0x61,saved&~3);ob(0x43,0xb0);ob(0x42,ticks);ob(0x42,ticks>>8);ob(0x61,(saved&~2)|1);
  u32 spin=0;while(!(ib(0x61)&0x20)&&++spin<4000000){}
  ob(0x61,saved);if(spin>=4000000)return IO;
 }return 0;
}
