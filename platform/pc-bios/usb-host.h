/* MultiOS / MultiPulti — native polled USB hosts. */
#ifndef MULTIOS_USB_HOST_H
#define MULTIOS_USB_HOST_H
#include "platform/pc-bios/pci.h"
#include "multios/hid.h"
#include "multios/usb_bot.h"
#ifndef MULTIOS_USB_HOST_LIMIT
#define MULTIOS_USB_HOST_LIMIT 1
#endif
#if MULTIOS_USB_HOST_LIMIT < 1 || MULTIOS_USB_HOST_LIMIT > 4
#error USB host limit must be 1..4
#endif
typedef struct UsbBoot UsbBoot;
struct UsbBoot {
 void *host;
 int (*control)(UsbBoot *,u8,u8,u16,u16,u16);
 int (*arm)(UsbBoot *);
 int (*bulk)(UsbBoot *,u8,u8 *,u32,u32 *,u8 *);
 int (*connected)(UsbBoot *);
 MultiUsbBot disk;
 u16 bulk_in_packet,bulk_out_packet;
 MultiUsbController *info;
 MultiHid hid;
 u8 address,speed,mps,endpoint,interval,toggle,port,configured;
 u8 setup[8] __attribute__((aligned(16)));
 u8 data[512] __attribute__((aligned(4096)));
 u8 report[8] __attribute__((aligned(16)));
};
static inline void usb_barrier(void){__asm__ volatile("":::"memory");}
static inline u32 usb_phys(const volatile void *p){return (u32)(uintptr_t)p;}
MultiUsbController *pc_usb_claim(const char *kind,u32 base);
int pc_usb_enumerate(UsbBoot *b);
int pc_usb_mass_attach(UsbBoot *b,u8 iface,u8 in,u8 out,u16 pin,u16 pout);
void pc_usb_mass_poll(UsbBoot *b);
void pc_usb_mass_reset(void);
int pc_usb_mmio(const MultiPciDevice *,u32 *);
void pc_uhci_init(void);int pc_uhci_key(void);void pc_uhci_stop(void);
void pc_ohci_init(void);int pc_ohci_key(void);void pc_ohci_stop(void);
void pc_ehci_init(void);int pc_ehci_key(void);void pc_ehci_stop(void);
#endif
