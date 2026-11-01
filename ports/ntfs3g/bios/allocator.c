/* vm2 studios | t.me/v2vproj — freestanding bounded i386 NTFS arena.
 * SPDX-License-Identifier: MIT. Requires verified E820 RAM [1MiB,30MiB).
 * No firmware/libc allocator, no magic probing of nonexistent physical RAM. */
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#ifdef MPNTFS_ARENA_TEST
static unsigned char arena[24u*1024u*1024u] __attribute__((aligned(16)));
#define BASE arena
#else
#define BASE ((unsigned char*)0x400000u)
#endif
#define BYTES (24u*1024u*1024u)
typedef struct Block {struct Block *prev,*next;size_t size;uint32_t free;} Block;
#define HDR ((sizeof(Block)+15u)&~15u)
static Block*head;
void mpntfs_reset(void){head=(Block*)BASE;head->prev=head->next=0;head->size=BYTES-HDR;head->free=1;}
void*mpntfs_malloc(size_t n){if(!head)mpntfs_reset();if(!n)n=1;if(n>BYTES-31){errno=ENOMEM;return 0;}n=(n+15)&~(size_t)15;
 for(Block*b=head;b;b=b->next)if(b->free&&b->size>=n){
  if(b->size>=n+HDR+16){Block*q=(Block*)((unsigned char*)b+HDR+n);q->prev=b;q->next=b->next;if(q->next)q->next->prev=q;q->size=b->size-n-HDR;q->free=1;b->next=q;b->size=n;}
  b->free=0;return (unsigned char*)b+HDR;
 }errno=ENOMEM;return 0;}
static void merge(Block*b){Block*n=b->next;if(n&&n->free){b->size+=HDR+n->size;b->next=n->next;if(b->next)b->next->prev=b;}}
void mpntfs_free(void*p){if(!p)return;Block*b=(Block*)((unsigned char*)p-HDR);b->free=1;merge(b);if(b->prev&&b->prev->free)merge(b->prev);}
void*mpntfs_calloc(size_t n,size_t size){if(size&&n>SIZE_MAX/size){errno=ENOMEM;return 0;}size_t bytes=n*size;unsigned char*p=mpntfs_malloc(bytes);if(p)for(size_t i=0;i<bytes;i++)p[i]=0;return p;}
void*mpntfs_realloc(void*p,size_t n){if(!p)return mpntfs_malloc(n);if(!n){mpntfs_free(p);return 0;}Block*b=(Block*)((unsigned char*)p-HDR);if(n<=b->size)return p;void*q=mpntfs_malloc(n);if(q){for(size_t i=0;i<b->size;i++)((unsigned char*)q)[i]=((unsigned char*)p)[i];mpntfs_free(p);}return q;}
char*mpntfs_strdup(const char*s){size_t n=0;while(s[n])n++;char*p=mpntfs_malloc(n+1);if(p)for(size_t i=0;i<=n;i++)p[i]=s[i];return p;}
