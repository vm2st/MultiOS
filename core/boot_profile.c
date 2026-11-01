/* vm2 studios | t.me/v2vproj — BIOS runtime profile */
#include "multios/boot_profile.h"
static u32 rd(const unsigned char *p){return (u32)p[0]|((u32)p[1]<<8)|((u32)p[2]<<16)|((u32)p[3]<<24);}
int multi_boot_profile(MultiProfile *p,const unsigned char s[512]){
 if(!p||!s)return INVALID;
 int nz=0;for(int i=0;i<512;i++)nz|=s[i];if(!nz)return 1;
 const unsigned char magic[8]={'M','P','C','F','G','2','5',0};
 for(int i=0;i<8;i++)if(s[i]!=magic[i])return INVALID;
 u32 c=~0u;for(int i=0;i<508;i++){c^=s[i];for(int j=0;j<8;j++)c=(c>>1)^((0u-(c&1))&0xedb88320u);}
 u32 n=rd(s+8);if(~c!=rd(s+508)||!n||n>492||rd(s+12)!=1)return INVALID;
 for(u32 i=16+n;i<508;i++)if(s[i])return INVALID;
 return multi_profile_parse(p,(const char *)s+16,n);
}
