/* vm2 studios | t.me/v2vproj — bounded BIOS EDD bootstrap + payload CRC32.
 * Boot device selected by BIOS; stage 2 uses EDD before protected mode. CRC detects corruption, NOT authenticity. */
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
static void stop(const char*s){volatile u16*v=(u16*)0xb8000;for(u32 i=0;s[i];i++)v[i]=0x4f00|(u8)s[i];for(;;)__asm__ volatile("cli;hlt");}
/* Stage-2 assembly read bounded sectors via BIOS EDD before protected mode. */
static const u8 *const header=(const u8 *)0x7200;
void loader_main(void){
 const char magic[8]={'M','P','R','W','2','2',0,0};for(u32 i=0;i<8;i++)if(header[i]!=(u8)magic[i])stop("MultiPulti vm2 studios: bad BIOS payload header");
 u32 n=*(u32*)(header+8),want=*(u32*)(header+12);if(n<16||n>(2048u-18)*512)stop("MultiPulti vm2 studios: BIOS payload size invalid");
#ifdef MULTIOS_BIOS_FAULT_TEST
 ((volatile u32*)0x7100)[0]=*(u32*)(header+16);((volatile u32*)0x7100)[1]=*(u32*)(header+20);
#endif
 u8*target=(u8*)0x100000;
 u32 crc=~0u;for(u32 i=0;i<n;i++){crc^=target[i];for(u32 j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(crc&1u));}
 if((~crc)!=want)stop("MultiPulti vm2 studios: BIOS payload CRC mismatch");
 ((void(*)(void))0x100000)();stop("MultiPulti vm2 studios: kernel returned");
}
