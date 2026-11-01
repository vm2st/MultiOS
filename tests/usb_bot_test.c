/* Test real BOT/SCSI implementation through deterministic mock USB pipes. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "multios/usb_bot.h"
typedef struct {int phase,connected,controls,cbws,opcode,wp,bad,ready_fail,no_sync,mode_fail,data_stall,csw_stall,clear_fail,cbw_stall,delays,mode_stall;u32 tag,n,bytes,last,sector,lba,mapped;u8 disk[512],footer[512];} Mock;
static u32 le(const u8 *p){return p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
static void put(u8 *p,u32 x){for(int i=0;i<4;i++)p[i]=x>>(8*i);}
static void be(u8 *p,u32 x){for(int i=0;i<4;i++)p[i]=x>>(8*(3-i));}
static int ctl(void *v,u8 type,u8 request,u16 value,u16 index,u16 n,u8 *d){Mock *m=v;(void)d;assert(!n&&!value);if(!m->controls)assert(type==0x21&&request==255&&index==3);else assert(type==2&&request==1&&(index==0x81||index==2));m->controls++;return m->bad==11||m->clear_fail?IO:0;}
static int connected(void *v){return ((Mock *)v)->connected;}
static int bulk(void *v,u8 ep,u8 *d,u32 n,u32 *actual,u8 *toggle){
 Mock *m=v;*actual=n;*toggle^=1;
 if(m->bad==10)return IO;
 if(m->phase==0&&m->cbw_stall){m->cbws++;*actual=0;return MULTI_USB_STALL;}
 if(m->phase==0){assert(ep==2&&n==31&&le(d)==0x43425355&&d[13]==0);m->cbws++;m->tag=le(d+4);m->n=le(d+8);m->opcode=d[15];m->lba=((u32)d[17]<<24)|((u32)d[18]<<16)|((u32)d[19]<<8)|d[20];m->bytes=0;m->phase=m->n?1:2;if(m->bad==8)*actual=30;return 0;}
 if(m->phase==1){assert(n==m->n);m->bytes=n;
  if(m->opcode==0x1a&&m->mode_stall)m->data_stall=1;
  if(m->data_stall){m->bytes=0;*actual=0;m->phase=2;return MULTI_USB_STALL;}
  if(m->opcode==0x2a){assert(ep==2&&n==512);memcpy(m->disk,d,n);}
  else{assert(ep==0x81);memset(d,0,n);switch(m->opcode){case 0x12:assert(n==36);d[0]=m->bad==12?5:0;break;case 0x25:be(d,m->last);be(d+4,m->sector);break;case 0x1a:d[0]=3;d[2]=m->wp?128:0;break;case 0x28:memcpy(d,m->mapped&&m->lba>=1022?m->footer:m->disk,n);break;case 3:d[0]=0x70;break;default:assert(0);}}
  if(m->bad==9)*actual=n+1;m->phase=2;return 0;
 }
 assert(m->phase==2&&ep==0x81&&n==13);if(m->csw_stall){m->csw_stall--;*actual=0;return MULTI_USB_STALL;}memset(d,0,n);put(d,0x53425355);put(d+4,m->tag);
 if(m->bad==1)d[0]^=1;if(m->bad==2)put(d+4,m->tag+1);if(m->bad==3)d[12]=2;if(m->bad==4)put(d+8,m->n+1);if(m->bad==5)put(d+8,1);if(m->bad==6)*actual=12;if(m->bad==7)d[12]=1;
 if(m->opcode==0&&m->ready_fail){d[12]=1;m->ready_fail--;}
 if((m->opcode==0x35&&m->no_sync)||(m->opcode==0x1a&&m->mode_fail))d[12]=1;
 if(m->data_stall){put(d+8,m->n);d[12]=1;m->data_stall=0;}
 m->phase=0;return 0;
}
static void delay(void *v,u32 ms){assert(ms==100);((Mock*)v)->delays++;}
static void init(Mock *m,MultiUsbBot *b){memset(m,0,sizeof(*m));m->connected=1;m->last=31;m->sector=512;memset(b,0,sizeof(*b));b->ctx=m;b->control=ctl;b->bulk=bulk;b->connected=connected;b->interface=3;b->ep_in=0x81;b->ep_out=2;b->delay=delay;}
static u32 crc(const u8 *p,u32 n){u32 c=~0u;for(u32 i=0;i<n;i++){c^=p[i];for(u32 j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1u));}return ~c;}
int main(void){
 Mock m;MultiUsbBot b;u8 x[512],y[512];for(int i=0;i<512;i++)x[i]=(u8)(i*17);
 init(&m,&b);assert(multi_bot_start(&b)==0&&b.writable&&b.sectors==32&&m.controls==3);
 assert(multi_bot_write(&b,7,x)==0);assert(multi_bot_flush(&b)==0);assert(multi_bot_read(&b,7,y)==0&&!memcmp(x,y,512));assert(b.reads==1&&b.writes==1&&b.flushes==1);
 int count=m.cbws;assert(multi_bot_read(&b,32,y)==INVALID&&multi_bot_write(&b,32,x)==INVALID&&m.cbws==count);assert(multi_bot_start(&b)==INVALID);
 b.tag=0xffffffffu;assert(multi_bot_read(&b,0,y)==0&&b.tag==1);
 for(int bad=1;bad<=10;bad++){init(&m,&b);assert(multi_bot_start(&b)==0);m.bad=bad;assert(multi_bot_write(&b,0,x)<0&&b.failed&&!b.writable);count=m.cbws;assert(multi_bot_write(&b,0,x)<0&&multi_bot_read(&b,0,y)<0&&multi_bot_flush(&b)<0&&m.cbws==count);}
 init(&m,&b);m.bad=11;assert(multi_bot_start(&b)<0&&b.failed);
 init(&m,&b);m.bad=12;assert(multi_bot_start(&b)==UNSUP&&!b.initialized);
 init(&m,&b);m.wp=1;assert(multi_bot_start(&b)==0&&!b.writable);assert(multi_bot_write(&b,0,x)==READONLY);assert(multi_bot_read(&b,0,y)==0);
 init(&m,&b);m.no_sync=1;assert(multi_bot_start(&b)==0&&!b.writable);
 init(&m,&b);m.mode_fail=1;assert(multi_bot_start(&b)==0&&!b.writable);
 init(&m,&b);m.mode_stall=1;assert(multi_bot_start(&b)==0&&!b.writable&&!b.failed&&m.controls==4);
 init(&m,&b);m.ready_fail=1;assert(multi_bot_start(&b)==0&&b.writable);
 init(&m,&b);m.ready_fail=3;assert(multi_bot_start(&b)==0&&b.writable&&m.delays==3);
 init(&m,&b);m.ready_fail=4;assert(multi_bot_start(&b)<0&&!b.initialized&&m.delays==3);
 init(&m,&b);m.last=0xffffffffu;assert(multi_bot_start(&b)==UNSUP);
 init(&m,&b);m.sector=4096;assert(multi_bot_start(&b)==UNSUP);
 init(&m,&b);assert(multi_bot_start(&b)==0);m.connected=0;count=m.cbws;assert(multi_bot_read(&b,0,y)<0&&b.failed&&count==m.cbws);m.connected=1;assert(multi_bot_write(&b,0,x)<0&&count==m.cbws);
 init(&m,&b);assert(multi_bot_start(&b)==0);m.no_sync=1;assert(multi_bot_flush(&b)<0&&b.failed&&!b.writable);
 init(&m,&b);b.ep_in=1;assert(multi_bot_start(&b)==INVALID);
 init(&m,&b);m.last=4095;assert(multi_bot_start(&b)==0);m.mapped=1;
 u8 mbr[512]={0};memcpy(mbr+432,"MP22\20\0\1\0",8);mbr[510]=0x55;mbr[511]=0xaa;
 mbr[450]=7;put(mbr+454,1);put(mbr+458,999);mbr[466]=0xda;put(mbr+470,1000);put(mbr+474,24);
 memcpy(m.footer,"MPSTORE1",8);put(m.footer+8,1);put(m.footer+12,512);put(m.footer+16,1024);put(m.footer+24,1000);put(m.footer+32,22);put(m.footer+76,crc(mbr,512));put(m.footer+508,crc(m.footer,508));
 u32 exposed=0;assert(multi_bot_image_extent(&b,mbr,&exposed)==0&&exposed==1024&&b.sectors==4096);
 m.footer[508]^=1;assert(multi_bot_image_extent(&b,mbr,&exposed)==BAD);m.footer[508]^=1;
 put(mbr+474,4096);assert(multi_bot_image_extent(&b,mbr,&exposed)==BAD);put(mbr+474,24);mbr[432]='X';assert(multi_bot_image_extent(&b,mbr,&exposed)==UNSUP);
 init(&m,&b);assert(multi_bot_start(&b)==0);count=m.cbws;m.csw_stall=1;
 assert(multi_bot_write(&b,0,x)==0&&m.cbws==count+1&&m.controls==4&&!b.failed&&!memcmp(m.disk,x,512));
 init(&m,&b);assert(multi_bot_start(&b)==0);count=m.cbws;m.csw_stall=2;
 assert(multi_bot_write(&b,0,x)<0&&b.failed&&m.cbws==count+1&&m.controls==4);
 init(&m,&b);assert(multi_bot_start(&b)==0);count=m.cbws;m.data_stall=1;
 assert(multi_bot_write(&b,0,x)<0&&b.failed&&m.cbws==count+1&&m.controls==4);
 init(&m,&b);assert(multi_bot_start(&b)==0);count=m.cbws;m.cbw_stall=1;
 assert(multi_bot_write(&b,0,x)<0&&b.failed&&m.cbws==count+1&&m.controls==3);
 init(&m,&b);assert(multi_bot_start(&b)==0);m.csw_stall=1;m.clear_fail=1;
 assert(multi_bot_write(&b,0,x)<0&&b.failed&&m.controls==4);
 init(&m,&b);assert(multi_bot_start(&b)==0);m.csw_stall=1;m.bad=2;
 assert(multi_bot_read(&b,0,y)<0&&b.failed);
 puts("PASS: bounded readiness, data/CBW/CSW STALL, clear-halt failure, one CSW retry, NEVER WRITE retry.");
 puts("PASS: BOT/SCSI RW roundtrip, flush, CBW/CSW, 10 transport/status faults, tag wrap, LBA, write protection, unsupported flush/geometry, sense, disconnect and sticky no-retry protection; validated image prefix on larger USB device.");return 0;
}
