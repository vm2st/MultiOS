/* MultiPulti | vm2 studios | t.me/v2vproj — bounded diagnostic inventory. */
#ifndef MULTIOS_HARDWARE_H
#define MULTIOS_HARDWARE_H
#include "multi.h"
#define MULTI_PCI_MAX 128
#define MULTI_USB_MAX 12
typedef struct { u16 vendor,device; u8 bus,slot,function,base_class,subclass,interface; } MultiPciDevice;
typedef struct { u16 io; u8 ports,keyboards; u32 reports,errors,disconnects; const char *status; u32 base; const char *kind; } MultiUsbController;
void multi_hardware_reset(void);
int multi_hardware_pci_add(const MultiPciDevice *d);
void multi_hardware_pci_finish(int scanned);
u32 multi_hardware_pci_count(void);
const MultiPciDevice *multi_hardware_pci(u32 index);
MultiUsbController *multi_hardware_usb(u32 index);
void multi_hardware_report(int usb_only);
#endif
