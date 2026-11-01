/* vm2 studios | t.me/v2vproj — firmware-protocol mocks, not proof of firmware glyph support. */
#include "platform/uefi/platform.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef uefi_call_wrapper
#define uefi_call_wrapper(fn,n,...) (fn)(__VA_ARGS__)
#define system_table text_system
#include "drivers/uefi/text.c"
#undef system_table
#define system_table key_system
#include "drivers/uefi/keys.c"
#undef system_table
#define decoder plain_decoder
#include "drivers/uefi/plain.c"
#undef decoder
static EFI_INPUT_KEY queue[16];static u32 qsize,qat;static CHAR16 captured[8192];static u32 capsize;
static EFI_STATUS takekey(EFI_SIMPLE_TEXT_IN_PROTOCOL *p,EFI_INPUT_KEY *k){(void)p;assert(qat<qsize);*k=queue[qat++];return EFI_SUCCESS;}
static EFI_STATUS output(SIMPLE_TEXT_OUTPUT_INTERFACE *p,CHAR16 *s){(void)p;while(*s){assert(capsize<8192);captured[capsize++]=*s++;}return EFI_SUCCESS;}
static EFI_STATUS position(SIMPLE_TEXT_OUTPUT_INTERFACE *p,UINTN c,UINTN r){(void)p;assert(c<80&&r<25);return EFI_SUCCESS;}
static EFI_STATUS attribute(SIMPLE_TEXT_OUTPUT_INTERFACE *p,UINTN a){(void)p;(void)a;return EFI_SUCCESS;}
static EFI_STATUS clearscreen(SIMPLE_TEXT_OUTPUT_INTERFACE *p){(void)p;return EFI_SUCCESS;}
int multi_osk_enabled(void){return 0;}int multi_osk_columns(int r){(void)r;return 0;}
const char *multi_osk_label(int r,int c,int s){(void)r;(void)c;(void)s;return "";}
void multi_osk_position(int*r,int*c,int*s){*r=*c=*s=0;}
void out(const char*s){(void)s;}void putch(char c){(void)c;}
int main(void){
 EFI_SIMPLE_TEXT_IN_PROTOCOL in={.ReadKeyStroke=takekey};SIMPLE_TEXT_OUTPUT_INTERFACE outp={.OutputString=output,.SetCursorPosition=position,.SetAttribute=attribute,.ClearScreen=clearscreen};
 EFI_SYSTEM_TABLE st={.ConIn=&in,.ConOut=&outp};text_system=key_system=&st;
 queue[qsize++]=(EFI_INPUT_KEY){0,0x0101};queue[qsize++]=(EFI_INPUT_KEY){0,0x416};
 queue[qsize++]=(EFI_INPUT_KEY){0,0xd83d};queue[qsize++]=(EFI_INPUT_KEY){0,0xde00};
 queue[qsize++]=(EFI_INPUT_KEY){1,0};queue[qsize++]=(EFI_INPUT_KEY){0,'A'};
 const u8 wanted[]={0xc4,0x81,0xd0,0x96,0xf0,0x9f,0x98,0x80};
 for(u32 i=0;i<sizeof(wanted);i++)assert(getkey(0)==wanted[i]);
 assert(getkey(0)==MULTI_KEY_UP);assert(getkey(0)=='A');
 const char *text="AЖ漢😀Z";for(u32 i=0;text[i];i++)emit(0,text[i]);
 assert(x==5&&y==0&&cells[0][0]=='A'&&cells[0][1]==0x416&&cells[0][2]==0x6f22&&cells[0][3]=='?'&&cells[0][4]=='Z');
 for(int n=0;n<24;n++)emit(0,'\n');assert(x==0&&y==23);clear(0);assert(!cells[0][1]&&!decoder.left);
 capsize=0;for(u32 i=0;text[i];i++)emit_plain(&st,text[i]);
 assert(capsize==5&&captured[1]==0x416&&captured[2]==0x6f22&&captured[3]=='?');
 puts("PASS UEFI protocol mocks: BMP + surrogate input encoded as UTF-8, no key-event collision; BMP output, non-BMP fallback, UTF-16 scrolling and clear. Glyph rendering still depends on firmware.");
}
