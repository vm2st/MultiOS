/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "multi.h"
void *mzero(void *p, u32 n) {
    u8 *d = p;
    while (n--)
        *d++ = 0;
    return p;
}
void mcopy(void *d, const void *s, u32 n) {
    u8 *a = d;
    const u8 *b = s;
    while (n--)
        *a++ = *b++;
}
u32 slen(const char *s) {
    u32 n = 0;
    while (s[n])
        n++;
    return n;
}
int streq(const char *a, const char *b) {
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
u8 upper(u8 c) {
    return c >= 'a' && c <= 'z' ? c - 32 : c;
}
int same(const char *a, const char *b) {
    while (*a && upper(*a) == upper(*b)) {
        a++;
        b++;
    }
    return *a == *b;
}
void number(u32 n) {
    char b[11];
    int k = 0;
    do {
        b[k++] = '0' + n % 10;
        n /= 10;
    } while (n);
    while (k)
        putch(b[--k]);
}
void hexnum(u32 n, int d) {
    const char *h = "0123456789ABCDEF";
    for (int i = d - 1; i >= 0; i--)
        putch(h[(n >> (i * 4)) & 15]);
}
const char *errstr(int e) {
    switch (e) {
    case IO:
        return "disk I/O failed; remount read-only and run fsck externally";
    case BAD:
        return "invalid/corrupt filesystem metadata (or dirty volume)";
    case UNSUP:
        return "unsupported filesystem feature/size/name";
    case NOTFOUND:
        return "not found";
    case EXISTS:
        return "already exists";
    case NOSPACE:
        return "disk or directory full";
    case READONLY:
        return "read-only; backend, driver or mount policy forbids writes";
    case NOTDIR:
        return "not a directory";
    case ISDIR:
        return "is a directory";
    case NOTEMPTY:
        return "directory not empty";
    case TOOLONG:
        return "path/line too long";
    case CANCELLED:
        return "copy cancelled; staging prefix retained for explicit resume";
    default:
        return "invalid arguments/name";
    }
}

/* Portable 64-bit decimal formatting: bitwise divide-by-ten avoids compiler runtime. */
void number64(uint64_t v){
    char digits[20];u32 n=0;
    do{uint64_t q=0;u32 rem=0;
        for(int bit=63;bit>=0;bit--){rem=(rem<<1)|(u32)((v>>bit)&1);if(rem>=10){rem-=10;q|=UINT64_C(1)<<bit;}}
        digits[n++]=(char)('0'+rem);v=q;
    }while(v);
    while(n)putch(digits[--n]);
}
void hexnum64(uint64_t v){hexnum((u32)(v>>32),8);hexnum((u32)v,8);}

#include "unicode.h"
#include "unicode_upcase.h"
int multi_utf8_feed(MultiUtf8State *s,u8 b,u32 *v){
 if(!s||!v)return INVALID;
 if(!s->left){
  if(b<128){*v=b;return 1;}
  if(b>=0xc2&&b<=0xdf){s->value=b&31;s->left=1;s->minimum=128;return 0;}
  if(b>=0xe0&&b<=0xef){s->value=b&15;s->left=2;s->minimum=2048;return 0;}
  if(b>=0xf0&&b<=0xf4){s->value=b&7;s->left=3;s->minimum=65536;return 0;}
  return INVALID;
 }
 if((b&0xc0)!=0x80){s->left=0;return INVALID;}
 s->value=(s->value<<6)|(b&63);
 if(--s->left)return 0;
 u32 c=s->value;if(c<s->minimum||c>0x10ffff||(c>=0xd800&&c<=0xdfff))return INVALID;
 *v=c;return 1;
}
int multi_utf8_to16(const char *s,u16 *out,u32 cap,u32 *units){
 if(!s||!units)return INVALID;
 /* Two passes: no partial output or count on malformed input/capacity failure. */
 u32 n=0;MultiUtf8State st={0};
 for(u32 i=0;s[i];i++){u32 c;int r=multi_utf8_feed(&st,(u8)s[i],&c);if(r<0)return INVALID;
  if(r){u32 add=c>=65536?2:1;if(n>cap||add>cap-n)return TOOLONG;n+=add;}}
 if(st.left)return INVALID;
 if(out){st=(MultiUtf8State){0};u32 j=0;
  for(u32 i=0;s[i];i++){u32 c;if(multi_utf8_feed(&st,(u8)s[i],&c)>0){
   if(c>=65536){c-=65536;out[j++]=0xd800+(c>>10);out[j++]=0xdc00+(c&1023);}else out[j++]=(u16)c;}}}
 *units=n;return 0;
}
static int scalar16(const u16 *s,u32 n,u32 *p,u32 *v){
 u32 c=s[(*p)++];if(c>=0xd800&&c<=0xdbff){if(*p>=n||s[*p]<0xdc00||s[*p]>0xdfff)return INVALID;
 c=65536+((c-0xd800)<<10)+(s[(*p)++]-0xdc00);}else if(c>=0xdc00&&c<=0xdfff)return INVALID;
 if(!c)return INVALID;*v=c;return 0;
}
int multi_utf16_to8(const u16 *s,u32 n,char *out,u32 cap){
 if(!s||!out||!cap)return INVALID;u32 bytes=0;
 for(u32 i=0;i<n;){u32 c;TRY(scalar16(s,n,&i,&c));u32 k=c<128?1:c<2048?2:c<65536?3:4;
 if(bytes>=cap||k>cap-1-bytes)return TOOLONG;bytes+=k;}
 u32 j=0;for(u32 i=0;i<n;){u32 c;TRY(scalar16(s,n,&i,&c));
 if(c<128)out[j++]=c;else if(c<2048){out[j++]=0xc0|(c>>6);out[j++]=0x80|(c&63);}
 else if(c<65536){out[j++]=0xe0|(c>>12);out[j++]=0x80|((c>>6)&63);out[j++]=0x80|(c&63);}
 else {out[j++]=0xf0|(c>>18);out[j++]=0x80|((c>>12)&63);out[j++]=0x80|((c>>6)&63);out[j++]=0x80|(c&63);}}
 out[j]=0;return 0;
}
int multi_name_entry(const u16 *s,u32 n,Entry *e){
 e->ascii=1;e->name_valid=0;for(u32 i=0;i<n;i++){if(s[i]<32||s[i]==127)return BAD;if(s[i]>126)e->ascii=0;}
 int r=multi_utf16_to8(s,n,e->name,sizeof(e->name));
 if(r==TOOLONG){const char *msg="[name exceeds UTF-8 path limit]";mcopy(e->name,msg,slen(msg)+1);return 0;}
 if(r<0)return BAD;e->name_valid=1;return 0;
}
int multi_name_valid(const char *s){
 if(!s)return INVALID;u32 l=slen(s),n;if(!l||l>255||s[l-1]==' '||s[l-1]=='.')return INVALID;
 TRY(multi_utf8_to16(s,0,255,&n));
 for(u32 i=0;i<l;i++){u8 c=s[i];if(c<32||c==127||c=='"'||c=='*'||c=='/'||c==':'||c=='<'||c=='>'||c=='?'||c=='\\'||c=='|')return INVALID;}
 return 0;
}
/* ADS uses separate storage: up to 255 UTF-16 units, not a 255-byte path. */
int multi_stream_name_valid(const char*s){
 if(!s)return INVALID;u32 l=slen(s),n;if(!l||l>765||s[l-1]==' '||s[l-1]=='.')return INVALID;
 TRY(multi_utf8_to16(s,0,255,&n));
 for(u32 i=0;i<l;i++){u8 c=s[i];if(c<32||c==127||c=='"'||c=='*'||c=='/'||c==':'||c=='<'||c=='>'||c=='?'||c=='\\'||c=='|')return INVALID;}
 return 0;
}
u32 multi_name_units(const char *s){u32 n=0;return multi_utf8_to16(s,0,255,&n)<0?0:n;}
int multi_default_upcase(u16 c,u16 *v){u32 lo=0,hi=sizeof(name_upcase)/sizeof(name_upcase[0]);
 while(lo<hi){u32 mid=lo+(hi-lo)/2;if(name_upcase[mid][0]<c)lo=mid+1;else hi=mid;}
 *v=lo<sizeof(name_upcase)/sizeof(name_upcase[0])&&name_upcase[lo][0]==c?name_upcase[lo][1]:c;return 0;
}
int multi_name_equal(const char *a,const char *b,MultiUpcase map){
 u16 x[255],y[255];u32 nx,ny;TRY(multi_utf8_to16(a,x,255,&nx));TRY(multi_utf8_to16(b,y,255,&ny));
 if(nx!=ny)return 0;for(u32 i=0;i<nx;i++){if(x[i]==y[i])continue;u16 p,q;TRY(map(x[i],&p));TRY(map(y[i],&q));if(p!=q)return 0;}return 1;
}
int multi_default_equal(const char *a,const char *b){return multi_name_equal(a,b,multi_default_upcase);}
