/* vm2 studios | t.me/v2vproj — checked 64-bit sector offsets without 64-bit division. */
#include "multios/block64.h"
int multi_block64_bind(MultiBlock64 *io,const MultiBlock64Provider *p) {
    if(!io || !p || io->bound || p->abi!=MULTIOS_BLOCK64_ABI || !p->sectors ||
       p->sector_bytes<512 || p->sector_bytes>4096 || (p->sector_bytes&(p->sector_bytes-1)) ||
       !p->read || (p->writable!=0 && p->writable!=1) || (p->writable && (!p->write || !p->flush))) return INVALID;
    io->provider=*p;io->failed=0;io->bound=1;return 0;
}
static int status(MultiBlock64 *io,int r) {
    if(r>0) r=IO;
    if(r<0) {io->failed=1;io->provider.writable=0;}
    return r;
}
int multi_block64_read(MultiBlock64 *io,uint64_t lba,u8 *b) {
    if(!io || !io->bound || !b) return INVALID;
    if(lba>=io->provider.sectors) return BAD;
    return status(io,io->provider.read(io->provider.ctx,lba,b));
}
int multi_block64_write(MultiBlock64 *io,uint64_t lba,const u8 *b) {
    if(!io || !io->bound || !b) return INVALID;
    if(lba>=io->provider.sectors) return BAD;
    if(io->failed || !io->provider.writable) return READONLY;
    return status(io,io->provider.write(io->provider.ctx,lba,b));
}
int multi_block64_flush(MultiBlock64 *io) {
    if(!io || !io->bound) return INVALID;
    /* A failed provider must not issue a later flush that persists partial I/O.
     * Reads remain available for diagnostics; recovery requires a fresh bind. */
    if(io->failed) return READONLY;
    if(!io->provider.flush) return io->provider.writable?IO:0;
    return status(io,io->provider.flush(io->provider.ctx));
}
int multi_block64_read_bytes(MultiBlock64 *io,uint64_t offset,u8 *out,u32 n,u8 *scratch,u32 scratch_bytes) {
    if(!io || !io->bound || (n && (!out || !scratch))) return INVALID;
    if(!n) return 0;
    u32 sector=io->provider.sector_bytes,shift=9;
    if(scratch_bytes<sector) return INVALID;
    while(((u32)1<<shift)<sector) shift++;
    if((uint64_t)(n-1)>UINT64_MAX-offset) return BAD;
    if((offset>>shift)>=io->provider.sectors || ((offset+n-1)>>shift)>=io->provider.sectors) return BAD;
    while(n) {
        u32 part=(u32)(offset&(sector-1)),take=sector-part;
        if(take>n) take=n;
        TRY(multi_block64_read(io,offset>>shift,scratch));
        for(u32 j=0;j<take;j++) out[j]=scratch[part+j];
        out+=take;n-=take;
        /* Last-byte access at UINT64_MAX is valid; do not increment after final chunk. */
        if(n) offset+=take;
    }
    return 0;
}
