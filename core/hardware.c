/* MultiPulti | vm2 studios | t.me/v2vproj */
#include "multios/hardware.h"
static MultiPciDevice pci[MULTI_PCI_MAX];
static MultiUsbController usb[MULTI_USB_MAX];
static u32 count,dropped;static int scanned;
void multi_hardware_reset(void){mzero(pci,sizeof(pci));mzero(usb,sizeof(usb));count=dropped=0;scanned=0;}
int multi_hardware_pci_add(const MultiPciDevice *d){
 if(!d||d->vendor==0xffff||d->slot>31||d->function>7)return INVALID;
 if(count==MULTI_PCI_MAX){dropped++;return NOSPACE;}
 pci[count++]=*d;return 0;
}
void multi_hardware_pci_finish(int s){scanned=!!s;}
u32 multi_hardware_pci_count(void){return count;}
const MultiPciDevice *multi_hardware_pci(u32 i){return i<count?&pci[i]:0;}
MultiUsbController *multi_hardware_usb(u32 i){return i<MULTI_USB_MAX?&usb[i]:0;}
void multi_hardware_report(int usb_only){
 if(!usb_only){
  out("PCI inventory | vm2 studios | t.me/v2vproj\n");
  if(!scanned)out("Native PCI scan unavailable on this platform.\n");
  for(u32 i=0;i<count;i++){
   const MultiPciDevice *d=&pci[i];hexnum(d->bus,2);putch(':');hexnum(d->slot,2);putch('.');number(d->function);
   out(" vendor=");hexnum(d->vendor,4);out(" device=");hexnum(d->device,4);
   out(" class=");hexnum(d->base_class,2);putch('/');hexnum(d->subclass,2);putch('/');hexnum(d->interface,2);putch('\n');
  }
  out("Stored devices: ");number(count);out("; omitted: ");number(dropped);putch('\n');
 }
 out("Native USB controller inventory (not firmware USB)\n");int found=0;
 for(u32 i=0;i<MULTI_USB_MAX;i++)if(usb[i].base){
  found=1;out(usb[i].kind?usb[i].kind:"USB");out(" base=");hexnum(usb[i].base,8);out(" ports=");number(usb[i].ports);
  out(" keyboards=");number(usb[i].keyboards);out(" reports=");number(usb[i].reports);
  out(" errors=");number(usb[i].errors);out(" disconnects=");number(usb[i].disconnects);
  out(" status=");out(usb[i].status?usb[i].status:"unknown");putch('\n');
 }
 if(!found)out("No native USB controller active. UEFI firmware input is separate.\n");
 out("Native: UHCI/OHCI/EHCI, direct root-port Boot HID and MSC BOT LUN0 disks.\n"
     "No xHCI, external hubs, UAS, Bluetooth or NKRO report parser.\n");
}
