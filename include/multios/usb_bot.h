/* MultiOS / MultiPulti: bounded USB Mass Storage BOT / SCSI, LUN 0. */
#ifndef MULTIOS_USB_BOT_H
#define MULTIOS_USB_BOT_H
#include "multios/hal.h"
#define MULTI_USB_STALL (-100) /* endpoint STALL, not timeout/disconnect */
typedef struct {
 void *ctx;
 int (*control)(void *,u8,u8,u16,u16,u16,u8 *);
 int (*bulk)(void *,u8,u8 *,u32,u32 *,u8 *);
 int (*connected)(void *);
 u8 interface,ep_in,ep_out,toggle_in,toggle_out;
 u8 initialized,failed,writable;
 u32 tag,sectors,commands,reads,writes,flushes;
 void (*delay)(void *,u32); /* optional bounded startup readiness delay */
} MultiUsbBot;
/* Exactly one 512-byte sector per call; no command retry after mutation. */
int multi_bot_start(MultiUsbBot *b);
int multi_bot_read(MultiUsbBot *b,u32 lba,u8 *sector);
int multi_bot_write(MultiUsbBot *b,u32 lba,const u8 *sector);
int multi_bot_flush(MultiUsbBot *b);
/* Validated bounded view of a prepared image copied to a larger device. */
int multi_bot_image_extent(MultiUsbBot *b,const u8 mbr[512],u32 *sectors);
#endif
