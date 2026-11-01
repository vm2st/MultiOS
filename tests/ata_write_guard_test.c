/* vm2 studios | t.me/v2vproj — port-level IDENTIFY/cache/status model. */
#define MULTIOS_PC_IO_TEST
#include "drivers/pc-bios/ata.c"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u16 identity[256];static u32 idx,commands,flush_commands,checks;static u8 status_byte=0x40;static int stuck;
u32 multi_test_io_read(u16 port,u8 width){(void)width;if(port==0x1f0){assert(idx<256);u16 v=identity[idx++];if(idx==256)status_byte=stuck?0x48:0x40;return v;}return port==0x1f7?status_byte:0x40;}
void multi_test_io_write(u16 port,u32 value,u8 width){(void)width;if(port==0x1f7){commands++;if(value==0xec){idx=0;status_byte=0x48;}else if(value==0xe7){flush_commands++;status_byte=0x40;}}}
#define CK(x) do{checks++;assert(x);}while(0)
static void seed(void){memset(identity,0,sizeof(identity));identity[49]=0x200;identity[60]=10000;identity[83]=0x4000;identity[87]=0x4000;stuck=0;status_byte=0x40;flush_commands=0;}
int main(void){u8 data[512]={0};seed();identity[82]=identity[85]=0x20;identity[83]|=0x1000;CK(!identify()&&can_write&&can_flush);CK(!pc_flush(0)&&flush_commands==1);
 seed();identity[82]=identity[85]=0x20;CK(!identify()&&!can_write);u32 before=commands;CK(pc_write(0,1,data)==READONLY&&commands==before);CK(!pc_flush(0)&&!flush_commands);
 seed();identity[82]=0x20;CK(!identify()&&can_write&&!can_flush);CK(!pc_flush(0)&&!flush_commands);
 seed();CK(!identify()&&can_write&&!can_flush);CK(!pc_flush(0)&&!flush_commands);
 seed();identity[83]=0;CK(!identify()&&!can_write);
 seed();identity[82]=0x20;identity[87]=0;CK(!identify()&&!can_write);
 seed();identity[106]=0x5000;identity[117]=2048;CK(identify()==UNSUP);
 seed();identity[0]=0x8000;CK(identify()==UNSUP);
 seed();stuck=1;CK(identify()==IO);CK(waitata(0)==IO);
 status_byte=0x41;CK(waitata(0)==IO);status_byte=0xff;CK(waitata(1)==IO);
 printf("PASS ATA write guards: %u assertions; valid flush, disabled/absent/enabled/unknown cache, 4Kn/ATAPI, stuck DRQ and error status. Port model, not hardware.\n",checks);return 0;}
