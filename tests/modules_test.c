#include "multios/modules.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
Volume vol;
static int fail_at=-1, invalid_at=-1, calls, closes, close_order[4];
static int contexts[4], stops, outputs;
static void emit(void *ctx,char c) { assert(ctx==&contexts[0]); (void)c; outputs++; }
static int getkey(void *ctx) { assert(ctx==&contexts[1]); return 'x'; }
static int readblock(void *ctx,u32 lba,u8 *b) { assert(ctx==&contexts[2]); (void)lba; memset(b,0,512); return 0; }
static void stop(void *ctx) { assert(ctx==&contexts[3]); stops++; }
static int open_kind(void *ctx,const char *options,MultiDevice *d,int k) {
    assert(ctx==contexts); assert(!*options); calls++;
    if (fail_at==k) return IO;
    if (invalid_at==k) return 0;
    if (k==0) d->output=(MultiOutput){.ctx=&contexts[0],.putc=emit};
    if (k==1) d->input=(MultiInput){.ctx=&contexts[1],.getc=getkey};
    if (k==2) d->block=(MultiBlock){.ctx=&contexts[2],.sectors=8,.sector_bytes=512,.read=readblock};
    if (k==3) d->power=(MultiPower){.ctx=&contexts[3],.stop=stop};
    return 0;
}
#define OPEN(n) static int open##n(void *p,const char *s,MultiDevice *d) {return open_kind(p,s,d,n);} 
OPEN(0) OPEN(1) OPEN(2) OPEN(3)
#define CLOSE(n) static void close##n(MultiDevice *d) { (void)d; close_order[closes++]=n; }
CLOSE(0) CLOSE(1) CLOSE(2) CLOSE(3)
static MultiDriver drivers[]={
    MULTI_DRIVER("test-output",0,"test",open0,close0),
    MULTI_DRIVER("test-input",1,"test",open1,close1),
    MULTI_DRIVER("test-block",2,"test",open2,close2),
    MULTI_DRIVER("test-power",3,"test",open3,close3)
};
static const MultiDriver *const registry[]={&drivers[0],&drivers[1],&drivers[2],&drivers[3]};
static MultiProfile defaults={.id={"test-output","test-input","test-block","test-power"},.osk=0};
static int start(MultiProfile *p) {return multi_modules_start(registry,4,p,contexts,"test platform","native","test inventory");}
int main(void) {
    MultiProfile p=defaults, before;
    const char *good=" # comment\r\n output = test-output \r\nblock.options=\\MULTIOS\\DATA.IMG\nosk=off\n";
    assert(multi_profile_parse(&p,good,strlen(good))==0);
    assert(!strcmp(p.options[2],"\\MULTIOS\\DATA.IMG")); assert(p.osk==0);
    const char *bad[]={"unknown=x","osk=yes","input=","input=../module","output=test-output\noutput=test-output","input x","[drivers]","input=test-input\ninput.options=a\ninput.options=b","osk=on\nosk=off","power=TEST","input=foo/bar"};
    for (u32 i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        before=p; assert(multi_profile_parse(&p,bad[i],strlen(bad[i]))<0); assert(!memcmp(&before,&p,sizeof(p)));
    }
    char nul[]={'o','s','k','=','o','n',0}; before=p;
    assert(multi_profile_parse(&p,nul,sizeof(nul))<0); assert(!memcmp(&before,&p,sizeof(p)));
    char longtext[2050]; memset(longtext,'x',sizeof(longtext));
    assert(multi_profile_parse(&p,longtext,sizeof(longtext))<0);
    assert(multi_profile_parse(&p,longtext,179)<0);
    char longoption[160]="block.options="; memset(longoption+14,'x',128); longoption[142]=0;
    assert(multi_profile_parse(&p,longoption,142)<0);
    assert(multi_profile_parse(0,"",0)<0);
    /* Deterministic malformed-byte smoke fuzz: parser must stay bounded and atomic. */
    u32 seed=12345;
    for (int run=0;run<3000;run++) {
        char text[2050]; seed=seed*1664525u+1013904223u; u32 size=seed%sizeof(text);
        for (u32 i=0;i<size;i++) { seed=seed*1664525u+1013904223u; text[i]=(char)(seed>>24); }
        p=defaults; before=p;
        if (multi_profile_parse(&p,text,size)<0) assert(!memcmp(&before,&p,sizeof(p)));
    }
    p=defaults;
    assert(multi_modules_start(0,4,&p,contexts,"test","native",0)<0);
    drivers[1].abi++; assert(start(&p)<0 && calls==0); drivers[1].abi--;
    drivers[1].size--; assert(start(&p)<0 && calls==0); drivers[1].size++;
    drivers[1].id="test-output"; assert(start(&p)<0 && calls==0); drivers[1].id="test-input";
    strcpy(p.id[1],"test-output"); assert(start(&p)==NOTFOUND && calls==0); p=defaults;
    strcpy(p.id[2],"absent"); assert(start(&p)==NOTFOUND && calls==0); p=defaults;
    memset(p.id[1],'a',sizeof(p.id[1])); assert(start(&p)<0 && calls==0); p=defaults;
    memset(p.options[0],'x',sizeof(p.options[0])); assert(start(&p)<0 && calls==0); p=defaults;
    p.osk=2; assert(start(&p)<0 && calls==0); p=defaults;
    for (int k=0;k<4;k++) {
        calls=closes=0; fail_at=k;
        assert(start(&p)==IO && !multi_hal()); assert(calls==k+1 && closes==k);
        for (int j=0;j<k;j++) assert(close_order[j]==k-j-1);
    }
    fail_at=-1;
    for (int k=0;k<4;k++) {
        calls=closes=0; invalid_at=k;
        assert(start(&p)<0 && !multi_hal()); assert(calls==k+1 && closes==k+1);
        for (int j=0;j<=k;j++) assert(close_order[j]==k-j);
    }
    invalid_at=-1; calls=closes=0; p.osk=1;
    assert(start(&p)==UNSUP && closes==4); p.osk=0;
    calls=closes=0;
    assert(start(&p)==0 && calls==4 && closes==0);
    assert(!strcmp(multi_modules_error(),"none"));
    putch('x'); assert(outputs==1); assert(input_char()=='x');
    u8 block[512]; assert(disk_read(0,block)==0 && disk_read(8,block)==IO);
    assert(disk_write(0,block)==READONLY);
    assert(multi_hal()->poweroff==0 && multi_hal()->reboot==0);
    multi_modules_list(); assert(outputs>1);
    platform_exit(); assert(stops==1);
    assert(start(&p)==INVALID && calls==4); /* no live replacement, even unmounted */
    puts("Module contract passed: atomic config, ABI, selection, rollback, isolated contexts, boot freeze.");
}
