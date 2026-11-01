/* vm2 studios | t.me/v2vproj — small firmware-only C runtime.
 * No host syscalls; UTF-8 conversion is implemented inside NTFS-3G.
 * Locale-dependent fallbacks explicitly fail instead of guessing an encoding.
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <wchar.h>

extern void*mpntfs_malloc(size_t);
_Static_assert(sizeof(void*)==4 && sizeof(time_t)==8,"BIOS NTFS ABI: i386 with 64-bit time");
static int error_number;
int *__errno_location(void){return &error_number;}
size_t strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
int memcmp(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]-y[i];return 0;}
void*memmove(void*d,const void*s,size_t n){unsigned char*a=d;const unsigned char*b=s;if((uintptr_t)a<(uintptr_t)b)for(size_t i=0;i<n;i++)a[i]=b[i];else while(n){n--;a[n]=b[n];}return d;}
int strcmp(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return (unsigned char)*a-(unsigned char)*b;}
int strncmp(const char*a,const char*b,size_t n){for(size_t i=0;i<n;i++){if(a[i]!=b[i])return (unsigned char)a[i]-(unsigned char)b[i];if(!a[i])break;}return 0;}
char*strchr(const char*s,int c){for(;;s++){if(*s==(char)c)return (char*)s;if(!*s)return 0;}}
char*strerror(int e){switch(e){case ENOMEM:return "out of firmware memory";case EIO:return "block I/O failure";case ENOENT:return "not found";case EROFS:return "read only";case EINVAL:return "invalid argument";case ENOSPC:return "no space";default:return "NTFS firmware error";}}
int ffs(int v){unsigned int n=(unsigned int)v;if(!n)return 0;int b=1;while(!(n&1)){n>>=1;b++;}return b;}
size_t __ctype_get_mb_cur_max(void){return 1;}
size_t mbrtowc(wchar_t*w,const char*s,size_t n,mbstate_t*p){(void)w;(void)s;(void)n;(void)p;errno=EILSEQ;return (size_t)-1;}
size_t mbsrtowcs(wchar_t*w,const char**s,size_t n,mbstate_t*p){(void)w;(void)s;(void)n;(void)p;errno=EILSEQ;return (size_t)-1;}
size_t wcrtomb(char*s,wchar_t w,mbstate_t*p){(void)s;(void)w;(void)p;errno=EILSEQ;return (size_t)-1;}
int mbsinit(const mbstate_t*p){(void)p;return 1;}
/* No BIOS wall-clock provider yet. Fixed UTC epoch is documented, not a
 * fabricated current date. time_t is 64-bit even on this i386 port. */
time_t time(time_t*out){time_t v=946684800;if(out)*out=v;return v;}
void*memcpy(void*d,const void*s,size_t n){unsigned char*a=d;const unsigned char*b=s;for(size_t i=0;i<n;i++)a[i]=b[i];return d;}
void*memset(void*d,int c,size_t n){unsigned char*a=d;for(size_t i=0;i<n;i++)a[i]=(unsigned char)c;return d;}
char*strcpy(char*d,const char*s){char*o=d;while((*d++=*s++));return o;}
char*strncpy(char*d,const char*s,size_t n){size_t i=0;for(;i<n&&s[i];i++)d[i]=s[i];for(;i<n;i++)d[i]=0;return d;}
size_t strnlen(const char*s,size_t n){size_t i=0;while(i<n&&s[i])i++;return i;}
char*strrchr(const char*s,int c){const char*p=0;do{if(*s==(char)c)p=s;}while(*s++);return (char*)p;}
/* Linux header ABI helpers used internally by upstream special-file metadata;
 * this does not expose device-file creation or Linux syscalls to the shell. */
unsigned int gnu_dev_major(unsigned long long d){return (unsigned int)(((d>>8)&0xfffu)|((d>>32)&0xfffff000u));}
unsigned int gnu_dev_minor(unsigned long long d){return (unsigned int)((d&0xffu)|((d>>12)&0xffffff00u));}
unsigned long long gnu_dev_makedev(unsigned int a,unsigned int b){return ((unsigned long long)(a&0xfffu)<<8)|((unsigned long long)(a&0xfffff000u)<<32)|(b&255u)|((unsigned long long)(b&0xffffff00u)<<12);}
