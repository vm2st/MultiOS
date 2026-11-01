/* vm2 studios | t.me/v2vproj — actual PS2 driver, fully mocked port I/O. */
#define MULTIOS_PC_IO_TEST 1
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "drivers/pc-bios/keys.c"
static u8 bytes[32];static u32 cursor,length,reads;static int init_count,usb_event=-2;
u32 multi_test_io_read(u16 p,u8 width){assert(width==1);reads++;if(p==0x3fd)return 0;if(p==0x64){assert(cursor<length);return 1;}assert(p==0x60&&cursor<length);return bytes[cursor++];}
void multi_test_io_write(u16 p,u32 v,u8 width){(void)p;(void)v;(void)width;assert(!"unexpected PS2 write");}
void multi_usb_stop(void){}
void multi_usb_init(void){init_count++;}int multi_usb_key(void){int e=usb_event;usb_event=-2;return e;}
int streq(const char *a,const char *b){return !strcmp(a,b);}
static int feed(const u8 *b,u32 n){assert(n<=sizeof(bytes));memcpy(bytes,b,n);cursor=0;length=n;int e=pc_getc(0);assert(cursor==length);return e;}
#define FEED(...) feed((const u8[]){__VA_ARGS__},sizeof((const u8[]){__VA_ARGS__}))
int main(void){
 MultiDevice d;assert(open_keys(0,"",&d)==0&&init_count==1);assert(open_keys(0,"invalid",&d)==INVALID);
 assert(FEED(0xe1,0x1d,0x45,0xe1,0x9d,0xc5,30)=='a');
 assert(FEED(60)==MULTI_KEY_OSK);
 assert(FEED(30)=='a');assert(FEED(42,30)=='A');assert(FEED(54,170,30)=='A');assert(FEED(182,30)=='a');
 assert(FEED(0xe0,29,46)==3);assert(FEED(0xe0,157,46)=='c');
 assert(FEED(58,30)=='A');assert(FEED(58,30)=='A');assert(FEED(186,58,30)=='a');
 assert(FEED(69,71)=='7');assert(FEED(197,69,71)==MULTI_KEY_HOME);
 assert(FEED(0xe0,28)=='\n');assert(FEED(0xe0,53)=='/');assert(FEED(0xe0,83)==MULTI_KEY_DELETE);
 assert(FEED(63)==MULTI_KEY_PREVIOUS);assert(FEED(64)==MULTI_KEY_NEXT);assert(FEED(65)==MULTI_KEY_SELECT);
 assert(FEED(66)==MULTI_KEY_HISTORY_PREV);assert(FEED(67)==MULTI_KEY_HISTORY_NEXT);
 assert(open_keys(0,"ps2-only",&d)==0&&init_count==1);assert(FEED(48)=='b');
 assert(open_keys(0,"usb-only",&d)==0&&init_count==2);reads=0;usb_event='U';assert(pc_getc(0)=='U'&&reads==0);
 puts("PS2 actual driver (mock I/O): independent shifts, right Ctrl, caps debounce, num/keypad, extended Enter/slash/Delete, F2/F5-F9, input profile validation and USB-only no-PS2/no-COM1 reads passed.");
}
