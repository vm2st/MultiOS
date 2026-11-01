/* vm2 studios | t.me/v2vproj */
#ifndef MULTIOS_PC_PCI_H
#define MULTIOS_PC_PCI_H
#include "multios/hardware.h"
u32 pc_pci_read(const MultiPciDevice *,u8 offset);
void pc_pci_write16(const MultiPciDevice *,u8 offset,u16 value);
void pc_pci_scan(void);
int pc_delay_ms(u32 milliseconds);
#endif
