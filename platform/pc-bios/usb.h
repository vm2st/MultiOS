/* vm2 studios | t.me/v2vproj */
#ifndef MULTIOS_PC_USB_H
#define MULTIOS_PC_USB_H
void multi_usb_init(void);
int multi_usb_key(void); /* nonblocking, -2 empty */
void multi_usb_stop(void);
#endif
