/* MultiOS / MultiPulti: USB BOT block provider, LUN 0, fixed boot identity.
 * No live rebind after disconnect/error. Never silently falls back to ATA. */
#include "platform/pc-bios/io.h"
#include "platform/pc-bios/usb-host.h"
#include "platform/pc-bios/usb.h"
static UsbBoot *selected;
static void delay(void *ctx,u32 ms){(void)ctx;(void)pc_delay_ms(ms);}
void pc_usb_mass_reset(void){selected=0;}
static int control(void *ctx,u8 type,u8 req,u16 value,u16 index,u16 n,u8 *data){
 UsbBoot *b=ctx;int e=b->control(b,type,req,value,index,n);
 if(e>=0&&n)for(u32 i=0;i<n;i++)data[i]=b->data[i];return e;
}
static int bulk(void *ctx,u8 ep,u8 *data,u32 n,u32 *actual,u8 *toggle){
 UsbBoot *b=ctx;return b->bulk(b,ep,data,n,actual,toggle);
}
static int connected(void *ctx){UsbBoot *b=ctx;return b->connected(b);}
int pc_usb_mass_attach(UsbBoot *b,u8 iface,u8 in,u8 out,u16 pin,u16 pout){
 if(selected&&selected!=b)return UNSUP;
 b->bulk_in_packet=pin;b->bulk_out_packet=pout;
 mzero(&b->disk,sizeof(b->disk));b->disk=(MultiUsbBot){.ctx=b,.control=control,.bulk=bulk,.connected=connected,.interface=iface,.ep_in=in,.ep_out=out,.delay=delay};
 int e=multi_bot_start(&b->disk);if(e<0){b->info->status="USB BOT disk rejected";return e;}
 selected=b;b->configured=2;b->info->keyboards=0;
 b->info->status=b->disk.writable?"USB BOT disk ready / RW capable":"USB BOT disk ready / RO only";return 0;
}
void pc_usb_mass_poll(UsbBoot *b){
 if(!b->connected(b)&&!b->disk.failed){b->disk.failed=1;b->disk.writable=0;b->info->disconnects++;b->info->status="USB disk disconnected / restart required";}
 if(b->disk.failed){(void)multi_hal_media_failed(b);if(b->connected(b))b->info->status="USB disk failed / restart required";}
}
static int read(void *ctx,u32 lba,u8 *sector){UsbBoot *b=ctx;int e=multi_bot_read(&b->disk,lba,sector);if(e<0){b->info->errors++;pc_usb_mass_poll(b);}return e;}
static int write(void *ctx,u32 lba,const u8 *sector){UsbBoot *b=ctx;int e=multi_bot_write(&b->disk,lba,sector);if(e<0){b->info->errors++;pc_usb_mass_poll(b);}return e;}
static int flush(void *ctx){UsbBoot *b=ctx;int e=multi_bot_flush(&b->disk);if(e<0){b->info->errors++;pc_usb_mass_poll(b);}return e;}
static int open_disk(void *ctx,const char *options,MultiDevice *d){
 (void)ctx;if(*options)return INVALID;multi_usb_init();
 if(!selected||!selected->disk.initialized||selected->disk.failed)return NOTFOUND;
 u8 mbr[512];TRY(read(selected,0,mbr));
 const volatile u8 *boot_id=(const volatile u8 *)(0x7c00+440);int nonzero=0;
 for(u32 i=0;i<4;i++){nonzero|=boot_id[i];if(mbr[440+i]!=boot_id[i])return NOTFOUND;}
 if(!nonzero||mbr[510]!=0x55||mbr[511]!=0xaa)return BAD;
 u32 exposed=selected->disk.sectors;int extent=multi_bot_image_extent(&selected->disk,mbr,&exposed);
 if(extent<0&&extent!=UNSUP)return extent;if(exposed>0x10000000u)return UNSUP;
 d->block=(MultiBlock){.ctx=selected,.sectors=exposed,.sector_bytes=512,.writable=selected->disk.writable,.read=read,.write=write,.flush=flush};return 0;
}
const MultiDriver multi_pc_usb_disk=MULTI_DRIVER("pc-usb-bot",MULTI_BLOCK,"native USB MSC BOT LUN0, SCSI512, strict CSW, boot MBR-ID guard",open_disk,0);
static int open_ro(void *ctx,const char *options,MultiDevice *d){TRY(open_disk(ctx,options,d));d->block.writable=0;d->block.write=0;return 0;}
const MultiDriver multi_pc_usb_disk_ro=MULTI_DRIVER("pc-usb-bot-ro",MULTI_BLOCK,"native USB BOT enforced read-only",open_ro,0);
