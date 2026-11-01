/* vm2 studios | t.me/v2vproj — diagnostic inventory boundaries. */
#include "multios/hardware.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static char text[32000];static u32 pos;
void putch(char c){assert(pos+1<sizeof(text));text[pos++]=c;text[pos]=0;}
void out(const char *s){while(*s)putch(*s++);}
int main(void){
 multi_hardware_reset();assert(!multi_hardware_pci_count());assert(!multi_hardware_pci(0));assert(!multi_hardware_usb(MULTI_USB_MAX));
 multi_hardware_report(0);assert(strstr(text,"unavailable"));pos=0;
 MultiPciDevice d={.vendor=0x8086,.device=0x7020,.base_class=12,.subclass=3};
 assert(multi_hardware_pci_add(0)==INVALID);d.slot=32;assert(multi_hardware_pci_add(&d)==INVALID);d.slot=0;
 for(int i=0;i<MULTI_PCI_MAX;i++){d.bus=i;assert(multi_hardware_pci_add(&d)==0);}
 assert(multi_hardware_pci_add(&d)==NOSPACE);assert(multi_hardware_pci_count()==128);
 multi_hardware_pci_finish(1);MultiUsbController *u=multi_hardware_usb(0);u->io=0xc000;u->base=0xc000;u->kind="UHCI";u->ports=2;u->keyboards=1;u->status="test";
 multi_hardware_report(0);assert(strstr(text,"omitted: 1"));assert(strstr(text,"keyboards=1"));assert(strstr(text,"class=0C/03/00"));
 multi_hardware_reset();assert(!multi_hardware_usb(0)->io&&!multi_hardware_pci_count());
 puts("Hardware inventory: unavailable platform, bounded 128 records, invalid input/overflow, USB diagnostics and reset passed.");
}
