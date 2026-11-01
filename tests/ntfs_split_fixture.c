/* vm2 studios | t.me/v2vproj — TEST ONLY, independently serialize split DATA.
 * Requires installed libntfs-3g development headers; never linked into MultiOS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ntfs-3g/volume.h>
#include <ntfs-3g/inode.h>
#include <ntfs-3g/attrib.h>
#include <ntfs-3g/attrlist.h>
#include <ntfs-3g/mft.h>
#include <ntfs-3g/runlist.h>
#include <ntfs-3g/dir.h>
#define CHECK(x) do {if(!(x)){perror(#x);exit(1);}}while(0)
int main(int argc,char **argv){
 struct stat st;CHECK(argc==2);CHECK(!lstat(argv[1],&st)&&S_ISREG(st.st_mode)&&st.st_nlink==1);
 ntfs_volume *v=ntfs_mount(argv[1],0x08000000);CHECK(v);
 ntfs_inode *ni=ntfs_pathname_to_inode(v,NULL,"/split.bin");CHECK(ni);
 CHECK(!ntfs_inode_add_attrlist(ni));
 ntfs_attr_search_ctx *ctx=ntfs_attr_get_search_ctx(ni,NULL);CHECK(ctx);
 CHECK(!ntfs_attr_lookup(AT_DATA,NULL,0,CASE_SENSITIVE,0,NULL,0,ctx));
 ATTR_RECORD *a=ctx->attr;CHECK(a->non_resident);
 runlist_element *rl=ntfs_mapping_pairs_decompress(v,a,NULL);CHECK(rl);
 CHECK(rl[0].vcn==0&&rl[0].lcn>=0&&rl[0].length>=2&&rl[1].length==0);
 s64 cut=rl[0].length/2,end=rl[0].length,lcn=rl[0].lcn;
 runlist_element first[2]={{0,lcn,cut},{cut,LCN_ENOENT,0}};
 runlist_element second[2]={{cut,lcn+cut,end-cut},{end,LCN_ENOENT,0}};
 u8 mp[64];CHECK(!ntfs_mapping_pairs_build(v,mp,sizeof(mp),first,0,NULL));
 CHECK(le32_to_cpu(a->length)-le16_to_cpu(a->mapping_pairs_offset)>=8);
 memset((u8*)a+le16_to_cpu(a->mapping_pairs_offset),0,le32_to_cpu(a->length)-le16_to_cpu(a->mapping_pairs_offset));
 /* Small fixture has a single 1-byte length / 2-byte LCN pair + terminator. */
 unsigned size=1+(mp[0]&15)+(mp[0]>>4)+1;
 CHECK(size<=le32_to_cpu(a->length)-le16_to_cpu(a->mapping_pairs_offset));
 memcpy((u8*)a+le16_to_cpu(a->mapping_pairs_offset),mp,size);a->highest_vcn=cpu_to_sle64(cut-1);
 ntfs_inode_mark_dirty(ctx->ntfs_ino);ntfs_attr_put_search_ctx(ctx);
 ntfs_inode *ext=ntfs_mft_record_alloc(v,ni);CHECK(ext);
 int off=ntfs_non_resident_attr_record_add(ext,AT_DATA,NULL,0,cut,16,0);CHECK(off>=0);
 a=(ATTR_RECORD*)((u8*)ext->mrec+off);
 CHECK(a->type==AT_DATA&&a->non_resident);
 CHECK(!ntfs_mapping_pairs_build(v,(u8*)a+le16_to_cpu(a->mapping_pairs_offset),le32_to_cpu(a->length)-le16_to_cpu(a->mapping_pairs_offset),second,cut,NULL));
 a->highest_vcn=cpu_to_sle64(end-1);ntfs_inode_mark_dirty(ext);
 /* record_add maintains the base inode's $ATTRIBUTE_LIST. */
 CHECK(!ntfs_inode_sync(ni));free(rl);CHECK(!ntfs_inode_close(ni));CHECK(!ntfs_umount(v,0));
 puts("Independent libntfs-3g: DATA split into two MFT records.");return 0;
}
