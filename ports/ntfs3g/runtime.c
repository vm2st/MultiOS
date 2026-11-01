/* vm2 studios | t.me/v2vproj — small firmware-only C runtime.
 * No host syscalls; UTF-8 conversion is implemented inside NTFS-3G.
 * Locale-dependent fallbacks explicitly fail instead of guessing an encoding.
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <wchar.h>
#include <efi.h>
#include <efilib.h>
extern void*mpntfs_malloc(size_t);
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
static int leap(unsigned y){return y%4==0&&(y%100!=0||y%400==0);}
time_t time(time_t*result){
 EFI_TIME t;time_t v=946684800; /* Documented 2000-01-01 fallback if RTC missing. */
 if(!EFI_ERROR(uefi_call_wrapper(RT->GetTime,2,&t,0))&&t.Year>=1970&&t.Year<=9999&&t.Month>=1&&t.Month<=12&&t.Day>=1&&t.Day<=31&&t.Hour<24&&t.Minute<60&&t.Second<60){
  static const unsigned char months[]={31,28,31,30,31,30,31,31,30,31,30,31};
  uint64_t days=0;for(unsigned y=1970;y<t.Year;y++)days+=365+leap(y);
  for(unsigned m=1;m<t.Month;m++)days+=months[m-1]+(m==2&&leap(t.Year));
  days+=t.Day-1;v=(time_t)(days*86400+t.Hour*3600+t.Minute*60+t.Second);
  if(t.TimeZone>=-1440&&t.TimeZone<=1440)v-=t.TimeZone*60;
 }
 if(result)*result=v;return v;
}
