/* MultiOS / MultiPulti: USB MSC Bulk-Only Transport, bounded SCSI disk.
 * No dynamic allocation, no OS/firmware calls, no write retry/fake flush.
 * Strict CSW identity/residue/status validation. Transport failure is sticky. */
#include "multios/usb_bot.h"
static void le32(u8 *p,u32 x){for(u32 i=0;i<4;i++)p[i]=x>>(i*8);}
static u32 getle(const u8 *p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void be32(u8 *p,u32 x){for(u32 i=0;i<4;i++)p[i]=x>>((3-i)*8);}
static u32 getbe(const u8 *p){return p[3]|((u32)p[2]<<8)|((u32)p[1]<<16)|((u32)p[0]<<24);}
static int fault(MultiUsbBot *b){b->failed=1;b->writable=0;return IO;}
static int present(MultiUsbBot *b){return b&&b->bulk&&b->control&&b->connected&&!b->failed&&b->connected(b->ctx);}
static int xfer(MultiUsbBot *b,u8 ep,u8 *data,u32 n,u32 *actual){
 if(!present(b))return fault(b);
 u8 *toggle=ep&128?&b->toggle_in:&b->toggle_out;
 int r=b->bulk(b->ctx,ep,data,n,actual,toggle);
 if(*actual>n||!b->connected(b->ctx))return fault(b);
 if(r==MULTI_USB_STALL)return r;if(r)return fault(b);
 return 0;
}
/* -IO with failed=0 is a valid SCSI command-failed CSW, not transport success. */
static int clear_halt(MultiUsbBot *b,u8 ep){
 if(!present(b)||b->control(b->ctx,2,1,0,ep,0,0)<0)return fault(b);
 *(ep&128?&b->toggle_in:&b->toggle_out)=0;return 0;
}
static int command(MultiUsbBot *b,const u8 *cdb,u8 nc,u8 *data,u32 n,int input){
 if(!b||!cdb||!nc||nc>16||n>512||(n&&!data))return INVALID;
 if(!present(b))return fault(b);
 u8 cbw[31],csw[13];for(u32 i=0;i<31;i++)cbw[i]=0;
 u32 tag=++b->tag;if(!tag)tag=++b->tag;
 le32(cbw,0x43425355);le32(cbw+4,tag);le32(cbw+8,n);cbw[12]=input?128:0;cbw[14]=nc;
 for(u32 i=0;i<nc;i++)cbw[15+i]=cdb[i];
 u32 actual=0;int r=xfer(b,b->ep_out,cbw,31,&actual);if(r<0||actual!=31)return fault(b);
 u32 data_actual=0;
 if(n){u8 ep=input?b->ep_in:b->ep_out;r=xfer(b,ep,data,n,&data_actual);
  if(r==MULTI_USB_STALL){TRY(clear_halt(b,ep));}else if(r<0)return r;
  else if(!input&&data_actual!=n)return fault(b);
 }
 actual=0;r=xfer(b,b->ep_in,csw,13,&actual);
 if(r==MULTI_USB_STALL){TRY(clear_halt(b,b->ep_in));actual=0;r=xfer(b,b->ep_in,csw,13,&actual);}
 if(r<0)return fault(b);
 if(actual!=13||getle(csw)!=0x53425355||getle(csw+4)!=tag||csw[12]>1)return fault(b);
 u32 residue=getle(csw+8);if(residue>n||data_actual!=n-residue)return fault(b);
 b->commands++;
 if(csw[12])return IO;
 if(residue)return fault(b);
 return 0;
}
static int sense(MultiUsbBot *b){u8 cdb[6]={3,0,0,0,18,0},d[18];return command(b,cdb,6,d,18,1);}
int multi_bot_start(MultiUsbBot *b){
 if(!b||!b->bulk||!b->control||!b->connected||!(b->ep_in&128)||(b->ep_in&0x70)||!(b->ep_in&15)||(b->ep_out&0xf0)||!b->ep_out||b->initialized||b->failed)return INVALID;
 if(!b->connected(b->ctx))return fault(b);
 b->writable=0;b->sectors=0;b->toggle_in=b->toggle_out=0;
 /* Reset BOT and both pipes before the first CBW, never during an in-flight write. */
 if(b->control(b->ctx,0x21,255,0,b->interface,0,0)<0||b->control(b->ctx,2,1,0,b->ep_in,0,0)<0||b->control(b->ctx,2,1,0,b->ep_out,0,0)<0)return fault(b);
 u8 cdb[16]={0},d[512];cdb[0]=0x12;cdb[4]=36;
 TRY(command(b,cdb,6,d,36,1));if((d[0]&31)!=0||(d[0]&0xe0))return UNSUP;
 for(u32 i=0;i<16;i++)cdb[i]=0;
 int ready=IO;
 for(u32 attempt=0;attempt<4;attempt++){
  ready=command(b,cdb,6,0,0,0);if(!ready||b->failed)break;
  TRY(sense(b));if(attempt<3&&b->delay)b->delay(b->ctx,100);
 }TRY(ready);
 cdb[0]=0x25;TRY(command(b,cdb,10,d,8,1));u32 last=getbe(d);
 if(last==0xffffffffu||!last||getbe(d+4)!=512)return UNSUP;b->sectors=last+1;
 /* MODE SENSE(6) header: fail closed if write protection is unknown. */
 for(u32 i=0;i<16;i++)cdb[i]=0;cdb[0]=0x1a;cdb[2]=0x3f;cdb[4]=4;
 int mode=command(b,cdb,6,d,4,1);if(b->failed)return IO;
 int protected=mode<0||d[0]<3||(d[2]&128);
 if(mode<0){(void)sense(b);if(b->failed)return IO;}
 b->initialized=1;
 if(!protected){
  for(u32 i=0;i<16;i++)cdb[i]=0;cdb[0]=0x35;
  if(command(b,cdb,10,0,0,0)==0)b->writable=1;
  else if(b->failed)return IO;
 }
 return 0;
}
int multi_bot_read(MultiUsbBot *b,u32 lba,u8 *sector){
 if(!b||!b->initialized||!sector||lba>=b->sectors)return INVALID;
 u8 cdb[10]={0x28};be32(cdb+2,lba);cdb[8]=1;int e=command(b,cdb,10,sector,512,1);
 if(e<0)return fault(b);b->reads++;return 0;
}
int multi_bot_write(MultiUsbBot *b,u32 lba,const u8 *sector){
 if(!b||!b->initialized||!sector||lba>=b->sectors)return INVALID;
 if(b->failed)return IO;if(!b->writable)return READONLY;
 u8 cdb[10]={0x2a};be32(cdb+2,lba);cdb[8]=1;int e=command(b,cdb,10,(u8 *)sector,512,0);
 if(e<0)return fault(b);b->writes++;return 0;
}
int multi_bot_flush(MultiUsbBot *b){
 if(!b||!b->initialized)return INVALID;if(!present(b))return fault(b);
 if(!b->writable)return READONLY;
 u8 cdb[10]={0x35};if(command(b,cdb,10,0,0,0)<0)return fault(b);
 b->flushes++;return 0;
}
/* A copied MultiOS image may occupy only a prefix of a larger USB stick.
 * Expose that declared image extent, not arbitrary trailing sectors. The
 * transaction engine then sees its original footer at the virtual disk end.
 * This does not resize any filesystem or allocate the remaining device. */
static u32 crc32(const u8 *p,u32 n){u32 c=~0u;for(u32 i=0;i<n;i++){c^=p[i];for(u32 j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1u));}return ~c;}
int multi_bot_image_extent(MultiUsbBot *b,const u8 mbr[512],u32 *sectors){
 if(!b||!b->initialized||!mbr||!sectors)return INVALID;
 const u8 magic[8]={'M','P','2','2',16,0,1,0};
 for(u32 i=0;i<8;i++)if(mbr[432+i]!=magic[i])return UNSUP;
 if(mbr[510]!=0x55||mbr[511]!=0xaa)return BAD;
 u32 end=0,jbase=0,jcount=0;
 for(u32 i=0;i<4;i++){
  const u8 *p=mbr+446+i*16;if(!p[4])continue;u32 first=getle(p+8),count=getle(p+12);
  if(!first||!count||count>b->sectors||first>b->sectors-count)return BAD;
  if(first+count>end)end=first+count;
  if(p[4]==0xda){if(jbase)return BAD;jbase=first;jcount=count;}
 }
 if(!jbase||jcount<6||jcount>8196||jbase+jcount!=end||end>0x10000000u)return BAD;
 u8 a[512],c[512];TRY(multi_bot_read(b,end-2,a));TRY(multi_bot_read(b,end-1,c));
 for(u32 i=0;i<512;i++)if(a[i]!=c[i])return BAD;
 const u8 footer[8]={'M','P','S','T','O','R','E','1'};for(u32 i=0;i<8;i++)if(a[i]!=footer[i])return BAD;
 if(getle(a+8)!=1||getle(a+12)!=512||getle(a+16)!=end||getle(a+20)||getle(a+24)!=jbase||getle(a+28)||getle(a+32)!=jcount-2||getle(a+36)||getle(a+76)!=crc32(mbr,512)||getle(a+508)!=crc32(a,508))return BAD;
 *sectors=end;return 0;
}
