/* vm2 studios | t.me/v2vproj */
#include "multios/boot_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned crc(const unsigned char*p,unsigned n){unsigned c=~0u;while(n--){c^=*p++;for(int k=0;k<8;k++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}return ~c;}
static void le(unsigned char*p,unsigned x){for(int i=0;i<4;i++)p[i]=(unsigned char)(x>>(i*8));}
int main(void){
 MultiProfile p={.osk=0},old=p;unsigned char a[512]={0},b[512];
 assert(multi_boot_profile(&p,a)==1&&!memcmp(&p,&old,sizeof(p)));
 memcpy(a,"MPCFG25",8);le(a+8,7);le(a+12,1);memcpy(a+16,"osk=on\n",7);le(a+508,crc(a,508));
 assert(!multi_boot_profile(&p,a)&&p.osk==1);
 for(int i=0;i<4096;i++){memcpy(b,a,512);b[i/8]^=1u<<(i%8);old=p;assert(multi_boot_profile(&p,b)<0&&!memcmp(&p,&old,sizeof(p)));}
 for(int k=0;k<6;k++){memcpy(b,a,512);if(k==0)le(b+8,0);if(k==1)le(b+8,493);if(k==2)le(b+12,2);if(k==3)b[100]=1;if(k==4)b[16]=0;if(k==5)b[0]='?';le(b+508,crc(b,508));old=p;assert(multi_boot_profile(&p,b)<0&&!memcmp(&p,&old,sizeof(p)));}
 assert(multi_boot_profile(0,a)<0&&multi_boot_profile(&p,0)<0);
 puts("PASS BIOS config: 4096 bit corruptions, CRC-valid invalid fields, empty fallback, valid slot and atomic refusal.");
}
