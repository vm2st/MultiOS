/* vm2 studios | t.me/v2vproj — per-operation allocation ownership.
 * Host harness uses libc only here; UEFI uses Boot Services pools, no host OS.
 * Reset is permitted only after all volume handles have been abandoned/closed.
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#ifdef MPNTFS_FIRMWARE
#include <efi.h>
#include <efilib.h>
static void*raw_alloc(size_t n){void*base=0;if(EFI_ERROR(uefi_call_wrapper(BS->AllocatePool,3,EfiLoaderData,(UINTN)(n+sizeof(void*)+15),&base)))return 0;
 uintptr_t aligned=((uintptr_t)base+sizeof(void*)+15)&~(uintptr_t)15;((void**)aligned)[-1]=base;return (void*)aligned;}
static void raw_free(void*p){if(p)uefi_call_wrapper(BS->FreePool,1,((void**)p)[-1]);}
#else
#include <stdlib.h>
static void*raw_alloc(size_t n){return malloc(n);}
static void raw_free(void*p){free(p);}
#endif
typedef union Block Block;
union Block {struct{Block*prev,*next;size_t bytes;}h;max_align_t align;};
static Block*head;static size_t used;
#define BUDGET (32u*1024u*1024u)
void*mpntfs_malloc(size_t n){
 if(!n)n=1;if(n>BUDGET||used>BUDGET-n||n>SIZE_MAX-sizeof(Block)){errno=ENOMEM;return 0;}
 Block*b=raw_alloc(sizeof(Block)+n);if(!b){errno=ENOMEM;return 0;}
 b->h.prev=0;b->h.next=head;b->h.bytes=n;if(head)head->h.prev=b;head=b;used+=n;return b+1;
}
void mpntfs_free(void*p){if(!p)return;Block*b=(Block*)p-1;if(b->h.prev)b->h.prev->h.next=b->h.next;else head=b->h.next;if(b->h.next)b->h.next->h.prev=b->h.prev;used-=b->h.bytes;raw_free(b);}
void*mpntfs_calloc(size_t n,size_t s){if(s&&n>SIZE_MAX/s){errno=ENOMEM;return 0;}size_t total=n*s;unsigned char*p=mpntfs_malloc(total);if(p)for(size_t i=0;i<total;i++)p[i]=0;return p;}
void*mpntfs_realloc(void*p,size_t n){if(!p)return mpntfs_malloc(n);if(!n){mpntfs_free(p);return 0;}Block*b=(Block*)p-1;void*q=mpntfs_malloc(n);if(!q)return 0;size_t old=b->h.bytes;unsigned char*d=q,*s=p;for(size_t i=0;i<n&&i<old;i++)d[i]=s[i];mpntfs_free(p);return q;}
void mpntfs_reset(void){while(head)mpntfs_free(head+1);}
char*mpntfs_strdup(const char*s){size_t n=0;while(s[n])n++;if(n==SIZE_MAX){errno=ENOMEM;return 0;}char*p=mpntfs_malloc(n+1);if(p)for(size_t i=0;i<=n;i++)p[i]=s[i];return p;}
#ifndef MPNTFS_FIRMWARE
#include <time.h>
time_t mpntfs_time(time_t*out){const char*s=getenv("MULTIOS_NTFS_TEST_TIME");time_t v=s?(time_t)1900000000:time(0);if(out)*out=v;return v;}
#endif
