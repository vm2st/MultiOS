/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "multios/hal.h"
#include "multios/fs_driver.h"
#include "multios/osk.h"
#include "multios/modules.h"
#include "multios/brand.h"
#include "multios/cpu.h"
#include "multios/hardware.h"
#include "multios/editor.h"
#include "multios/copy.h"
#ifdef MULTIOS_STORAGE_TX
#include "multios/storage_tx.h"
#endif
static char cwd[256] = "/", line[512], last[512];
static int running = 1;
#define HISTORY_COUNT 8
static char history[HISTORY_COUNT][512];static u32 history_count,history_next;
static void history_add(const char *s){
 if(history_count&&streq(history[(history_next+HISTORY_COUNT-1)%HISTORY_COUNT],s))return;
 mcopy(history[history_next],s,slen(s)+1);history_next=(history_next+1)%HISTORY_COUNT;if(history_count<HISTORY_COUNT)history_count++;
}
static void back(u32 n){while(n--)putch('\b');}
/* Monospace scalar count; combining/wide glyph layout needs a future terminal driver. */
static u32 columns(const char *s,u32 bytes){u32 n=0;for(u32 i=0;i<bytes;i++)if(((u8)s[i]&0xc0)!=0x80)n++;return n;}
static void edit_paint(MultiEdit *e,u32 oldlen,u32 oldpos,int flags){
 u32 len=columns(e->text,e->length),pos=columns(e->text,e->cursor);
 if(flags&MULTI_EDIT_CHANGED){
  if(oldpos==oldlen&&pos==len&&len==oldlen+1){u32 at=e->length-1;while(at&&((u8)e->text[at]&0xc0)==0x80)at--;out(e->text+at);return;}
  if(oldpos==oldlen&&pos==len&&oldlen==len+1){out("\b \b");return;}
  back(oldpos);out(e->text);u32 end=len;while(end<oldlen){putch(' ');end++;}back(end-pos);
 }else if(flags&MULTI_EDIT_MOVED){
  back(oldpos);for(u32 i=0;i<e->cursor;i++)putch(e->text[i]);
 }
}

static void nl(void) {
    putch('\n');
}
static int pathof(const char *arg, char *dst) {
    char tmp[512];
    u32 n = 0;
    if (arg[0] != '/' && arg[0] != '\\') {
        u32 l = slen(cwd);
        mcopy(tmp, cwd, l);
        n = l;
        if (n > 1)
            tmp[n++] = '/';
    }
    for (u32 j = 0; arg[j]; j++) {
        if (n >= sizeof(tmp) - 1)
            return TOOLONG;
        tmp[n++] = arg[j] == '\\' ? '/' : arg[j];
    }
    tmp[n] = 0;
    u32 o = 1, i = 0;
    dst[0] = '/';
    dst[1] = 0;
    while (i < n) {
        while (tmp[i] == '/')
            i++;
        u32 start = i;
        while (tmp[i] && tmp[i] != '/')
            i++;
        u32 l = i - start;
        if (!l)
            break;
        if (l == 1 && tmp[start] == '.')
            continue;
        if (l == 2 && tmp[start] == '.' && tmp[start + 1] == '.') {
            while (o > 1 && dst[o - 1] != '/')
                o--;
            if (o > 1)
                o--;
            dst[o] = 0;
            continue;
        }
        if (o > 1) {
            if (o >= 255)
                return TOOLONG;
            dst[o++] = '/';
        }
        if (l > 255 - o)
            return TOOLONG;
        mcopy(dst + o, tmp + start, l);
        o += l;
        dst[o] = 0;
    }
    return 0;
}
static int get(const char *arg, char *path, Entry *e) {
    Stream p;
    int root;
    TRY(pathof(arg, path));
    return fs_resolve(path, &p, e, &root);
}
static int isancestor(const char *path, const char *child) {
    u32 n = slen(path);
    if (streq(path, "/"))
        return 1;
    if (slen(child) < n)
        return 0;
    for (u32 i = 0; i < n; i++)
        if (upper(path[i]) != upper(child[i]))
            return 0;
    return child[n] == 0 || child[n] == '/';
}
static int tokenize(char *s, char **v) {
    int n = 0;
    char *dst = s;
    while (*s) {
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s)
            break;
        if (n == 16)
            return -1;
        v[n++] = dst;
        int quote = 0;
        while (*s) {
            char c = *s++;
            if (c == '"') {
                quote = !quote;
                continue;
            }
            if (!quote && (c == ' ' || c == '\t'))
                break;
            *dst++ = c;
        }
        if (quote)
            return -1;
        *dst++ = 0;
    }
    return n;
}
static void help(void) {
    out("MultiOS " MULTIOS_VERSION " | DOS-like shell | ASCII commands / UTF-8 paths\n"
        "Volumes: FS [DRIVERS], MOUNT AUTO|RAW|1|2|3|4 (one volume; resets current path)\n"
        "Read:    DIR [path], CD [path], PWD, TREE [path], TYPE file, MORE file\n"
        "         HEX file, STAT path, DF, CRC32 file, CMP file file\n"
        "Wide RO: STAT64 file, READAT file offset count, HEXAT file offset count\n"
        "         decimal or 0x offset/count; count <=4096; CRC32/CMP use 64-bit views.\n"
        "Search:  FIND text file (case-sensitive, line <=511 bytes)\n"
        "Write:   UNLOCK YES (bounded allocation audit first), LOCK, TOUCH path\n"
        "         WRITE path \"text\", APPEND path \"text\" (adds LF)\n"
        "         COPY source dest (verified staged copy in TX profiles)\n"
        "         COPYRESUME source dest staging, COPYATOMIC source dest\n"
        "         COPYPAUSE source dest bytes (TX: retain prefix for resume)\n"
        "         WRITEAT file offset text (exact bytes, no LF), RESIZE file bytes\n"
        "NTFS: STREAMS file, SREAD/SHEX file stream offset count (<=4096)\n"
        "      SNEW/SWRITE/SAPPEND file stream text, SWRITEAT file stream offset text\n"
        "      COPYADS source dest, COPYADSRESUME source dest staging (DATA+ADS only)\n"
        "      SRESIZE file stream bytes, SDEL file stream, LINK source newpath\n"
        "         REN path newname (copies never overwrite existing files)\n"
        "         MD path, DEL file, RD empty-directory\n"
        "Script: SOURCE file (own command script, <=4096 bytes, depth <=4)\n"
        "Edit: Left/Right Home/End Delete; Ctrl+A/E/B/F/D; F8/F9 or Ctrl+P/N history\n"
        "Shell:   HELP, VER, PLATFORM, DRIVERS, MODULES, CREDITS, CPU, DEVICES, USB, CLS, ECHO text, HISTORY, SYNC, EXIT\n"
        "Access: F6 Next + F7 Select: complete two-button OSK control\n"
        "        OSK ON|OFF|TOGGLE; F2/Ctrl+O toggles the panel\n"
        "Power: POWEROFF/SHUTDOWN, REBOOT (when the platform supports it)\n"
        "Aliases: LS CAT CP RM MKDIR RMDIR CLEAR ?\n"
        "Paths: / absolute, relative, . and .., \\ separator, quotes for spaces.\n"
        "No executables, pipes, redirection, globbing or networking yet.\n"
        "All errors after a write starts lock the volume dirty; use external fsck.\n");
}
static int list(const char *arg, int tree, int depth) {
    Entry e;
    char path[256];
    TRY(get(arg, path, &e));
    if (!(e.attr & ATTR_DIR))
        return NOTDIR;
    u32 p = 0, count = 0;
    int r;
    Stream d = e.data;
    while ((r = fs_next(d, &p, &e)) > 0) {
        for (int j = 0; j < depth; j++)
            out("  ");
        out(e.attr & ATTR_DIR ? "<DIR> " : "      ");
        out(e.name);
        if (!e.name_valid && !e.ascii)
            out(" [name/codepage/path limit: lookup unsupported]");
        if (!(e.attr & ATTR_DIR)) {
            out("  ");
            if(e.large&~MULTI_ENTRY_SIZE64)out("[unsupported stream/feature]");
            else {number64(e.size64);if(e.large)out(" [64-bit read: STAT64/READAT/HEXAT]");}
        }
        nl();
        count++;
        if (tree && (e.attr & ATTR_DIR)) {
            if (depth >= 8) {
                out("  [depth limit 8]\n");
                continue;
            }
            char sub[256];
            u32 a = slen(path), b = slen(e.name);
            if (a + b + 2 > 256) {
                out("[path too long]\n");
                continue;
            }
            if (!e.name_valid && !e.ascii)
                continue;
            mcopy(sub, path, a);
            if (a > 1)
                sub[a++] = '/';
            mcopy(sub + a, e.name, b + 1);
            int rr = list(sub, 1, depth + 1);
            if (rr < 0)
                return rr;
        }
    }
    if (r < 0)
        return r;
    if (!tree) {
        number(count);
        out(" entries\n");
    }
    return 0;
}
static int unsigned64(const char *s,uint64_t *outvalue){
    if(!s||!*s||!outvalue)return INVALID;
    u32 base=10;if(s[0]=='0'&&(s[1]=='x'||s[1]=='X')){base=16;s+=2;if(!*s)return INVALID;}
    uint64_t value=0;
    while(*s){u8 c=(u8)*s++;u32 d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;
        if(d>=base)return INVALID;
        /* Constants avoid compiler-provided 64-bit division on i386. */
        uint64_t limit=base==16?UINT64_C(0x0fffffffffffffff):UINT64_C(1844674407370955161);
        if(value>limit || (base==10&&value==limit&&d>5))return INVALID;
        value=value*base+d;
    }
    *outvalue=value;return 0;
}
#ifdef MULTIOS_STORAGE_TX
static int pause_copy(void *ctx,uint64_t off,uint64_t total,int phase){
    (void)total;
    return phase==MULTIOS_COPY_DATA && off>=*(const uint64_t*)ctx;
}
#endif
static int wide_command(int argc,char **v){
    int stat=same(v[0],"stat64"),hex=same(v[0],"hexat");
    if(argc!=(stat?2:4))return INVALID;
    char path[256];MultiFile64 f;TRY(pathof(v[1],path));TRY(fs_open64(path,&f));
    if(stat){out("Path: ");out(path);out("\nSize64: ");number64(f.size);out("\nValid64: ");number64(f.valid);out("\nExtended read-only view\n");return 0;}
    uint64_t off,count;TRY(unsigned64(v[2],&off));TRY(unsigned64(v[3],&count));
    if(count>4096)return INVALID;
    if(off>f.size||count>f.size-off)return BAD;
    u8 b[256];u32 left=(u32)count;
    while(left){u32 take=left>sizeof(b)?sizeof(b):left;TRY(fs_read64(&f,off,b,take));
        if(hex){for(u32 i=0;i<take;i+=16){hexnum64(off+i);out("  ");for(u32 j=0;j<16&&i+j<take;j++){hexnum(b[i+j],2);putch(' ');}nl();}}
        else for(u32 i=0;i<take;i++){u8 c=b[i];putch(c=='\n'||c=='\r'||c=='\t'||(c>=32&&c<127)?c:'.');}
        off+=take;left-=take;
    }
    if(!hex)nl();return 0;
}
static int dump(const char *arg, int hex) {
    char path[256];
    Entry e;
    TRY(get(arg, path, &e));
    if (e.attr & ATTR_DIR)
        return ISDIR;
    u8 b[512];
    for (u32 off = 0; off < e.data.size;) {
        u32 n = e.data.size - off;
        if (n > 512)
            n = 512;
        TRY(fs_read(e.data, off, b, n));
        if (!hex) {
            for (u32 i = 0; i < n; i++) {
                u8 c = b[i];
                if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127))
                    putch(c);
                else
                    putch('.');
            }
        } else
            for (u32 i = 0; i < n; i += 16) {
                hexnum(off + i, 8);
                out("  ");
                for (u32 j = 0; j < 16; j++) {
                    if (i + j < n) {
                        hexnum(b[i + j], 2);
                        putch(' ');
                    } else
                        out("   ");
                }
                putch(' ');
                for (u32 j = 0; j < 16 && i + j < n; j++)
                    putch(b[i + j] >= 32 && b[i + j] < 127 ? b[i + j] : '.');
                nl();
            }
        off += n;
    }
    if (!hex)
        nl();
    return 0;
}
static int more(const char *arg){
 char path[256];Entry e;TRY(get(arg,path,&e));if(e.attr&ATTR_DIR)return ISDIR;
 u8 b[512];u32 rows=0,col=0;
 for(u32 off=0;off<e.data.size;){
  u32 n=e.data.size-off;if(n>512)n=512;TRY(fs_read(e.data,off,b,n));
  for(u32 i=0;i<n;i++){
   u8 ch=b[i];if(ch=='\n'){rows++;col=0;}else if(ch=='\r')col=0;else if(ch=='\t'){col=(col+8)&~7u;}else col++;
   if(col>=79){rows++;col=0;}
   putch(ch=='\n'||ch=='\r'||ch=='\t'||(ch>=32&&ch<127)?ch:'.');
   if(rows>=12&&off+i+1<e.data.size){
    out("\n-- MORE: F6/Space next; F7/Q/Esc quit --");int quit=0;
    for(;;){int k=multi_hal()->console.getc(multi_hal()->console.ctx);
     if(k<0||k==MULTI_KEY_SELECT||k==27||k==3||k=='q'||k=='Q'){quit=1;break;}
     if(k==MULTI_KEY_NEXT||k==' '||k=='\r'||k=='\n')break;
    }
    nl();if(quit)return 0;rows=col=0;
   }
  }off+=n;
 }nl();return 0;
}
static int execute(int argc,char **v);
static u32 script_depth;
static int source(const char *arg){
 if(script_depth>=4)return UNSUP;char path[256];Entry e;TRY(get(arg,path,&e));
 if(e.attr&ATTR_DIR)return ISDIR;if(e.large||e.data.size>4096)return TOOLONG;
 char text[4097];TRY(fs_read(e.data,0,(u8 *)text,e.data.size));text[e.data.size]=0;
 for(u32 i=0;i<e.data.size;i++)if((u8)text[i]>126||((u8)text[i]<32&&text[i]!='\n'&&text[i]!='\r'&&text[i]!='\t'))return INVALID;
 script_depth++;int result=0;u32 pos=0,ln=0;
 while(pos<e.data.size&&running){
  char command[512];u32 n=0;ln++;
  while(pos<e.data.size&&text[pos]!='\n'){
   if(n>=sizeof(command)-1){result=TOOLONG;goto done;}command[n++]=text[pos++];
  }if(pos<e.data.size)pos++;if(n&&command[n-1]=='\r')n--;command[n]=0;
  char *first=command;while(*first==' '||*first=='\t')first++;
  if(!*first||*first=='#'||*first==';')continue;
  char *args[16];int argc=tokenize(first,args);
  if(argc<0){result=INVALID;goto done;}if(argc)result=execute(argc,args);
  if(result<0)goto done;
 }
 done:script_depth--;if(result<0){out("SOURCE stopped at line ");number(ln);nl();}return result;
}
static int crc(const char *arg) {
    char path[256];
    MultiFile64 e;
    TRY(pathof(arg, path));
    TRY(fs_open64(path, &e));
    u32 c = 0xffffffffu;
    u8 b[512];
    for (uint64_t off = 0; off < e.size;) {
        u32 n = e.size - off > 512 ? 512 : (u32)(e.size - off);
        TRY(fs_read64(&e, off, b, n));
        for (u32 i = 0; i < n; i++) {
            c ^= b[i];
            for (int j = 0; j < 8; j++)
                c = (c >> 1) ^ ((0u - (c & 1)) & 0xedb88320u);
        }
        off += n;
    }
    hexnum(~c, 8);
    nl();
    return 0;
}
static int compare(const char *a, const char *b) {
    char p[256];
    MultiFile64 x, y;
    TRY(pathof(a, p)); TRY(fs_open64(p, &x));
    TRY(pathof(b, p)); TRY(fs_open64(p, &y));
    if (x.size != y.size) {
        out("Different sizes\n");
        return 0;
    }
    u8 aa[512], bb[512];
    for (uint64_t off = 0; off < x.size;) {
        u32 n = x.size - off > 512 ? 512 : (u32)(x.size - off);
        TRY(fs_read64(&x, off, aa, n));
        TRY(fs_read64(&y, off, bb, n));
        for (u32 j = 0; j < n; j++)
            if (aa[j] != bb[j]) {
                out("Different at byte ");
                number64(off + j);
                nl();
                return 0;
            }
        off += n;
    }
    out("Identical\n");
    return 0;
}
static int contains(const char *s, const char *q) {
    if (!*q)
        return 1;
    for (; *s; s++) {
        u32 j = 0;
        while (q[j] && s[j] == q[j])
            j++;
        if (!q[j])
            return 1;
    }
    return 0;
}
static int findtext(const char *q, const char *arg) {
    char path[256];
    Entry e;
    TRY(get(arg, path, &e));
    if (e.attr & ATTR_DIR)
        return ISDIR;
    u8 b[512];
    char text[512];
    u32 len = 0, line_no = 1;
    int clipped = 0;
    for (u32 off = 0; off < e.data.size;) {
        u32 n = e.data.size - off;
        if (n > 512)
            n = 512;
        TRY(fs_read(e.data, off, b, n));
        for (u32 j = 0; j < n; j++) {
            u8 c = b[j];
            if (c == '\n') {
                text[len] = 0;
                if (contains(text, q)) {
                    number(line_no);
                    out(": ");
                    out(text);
                    if (clipped)
                        out(" [truncated]");
                    nl();
                }
                len = 0;
                clipped = 0;
                line_no++;
            } else if (c != '\r') {
                if (len < 511)
                    text[len++] = (c >= 32 && c < 127) ? c : '.';
                else
                    clipped = 1;
            }
        }
        off += n;
    }
    if (len) {
        text[len] = 0;
        if (contains(text, q)) {
            number(line_no);
            out(": ");
            out(text);
            if (clipped)
                out(" [truncated]");
            nl();
        }
    }
    return 0;
}
static int execute(int argc, char **v) {
    char p[256], q[256];
    Entry e;
    char *c = v[0];
    if (same(c, "help") || same(c, "?")) {
        help();
        return 0;
    }
    if (same(c, "credits") || same(c, "about")) {
        out("MultiOS / " MULTIPULTI_NAME "\n" MULTIOS_CREDIT "\n"
            "Modular x86/x64 porting template. Not universal hardware support.\n"
            "Own C core under MIT; GNU-EFI and other upstream licenses retained.\n");
        return 0;
    }
    if (same(c, "cpu")) {
        multi_cpu_report();
        return 0;
    }
    if (same(c,"devices") || same(c,"pci") || same(c,"usb")) {
        if(argc!=1)return INVALID;multi_hardware_report(same(c,"usb"));return 0;
    }
    if (same(c, "ver")) {
        out("MultiOS " MULTIOS_VERSION "; portable C core; exFAT/FAT32 experimental RW; ");
#ifdef MULTIOS_NTFS3G
#ifdef MPNTFS_BIOS
        out("NTFS-3G experimental RW (native BIOS i386)\n");
#else
        out("NTFS-3G experimental RW (UEFI/host)\n");
#endif
#else
        out("NTFS RO\n");
#endif
        out(MULTIOS_STATUS "\n");
        out("Core: " MULTIPULTI_NAME " | " MULTIOS_CREDIT "\n");
        out(multi_hal()->name);
        putch('\n');
        return 0;
    }
    if (same(c, "platform") || same(c, "hal")) {
        const MultiHal *h = multi_hal();
        out("Platform: ");
        out(h->name);
        out("\nArchitecture: ");
        out(h->architecture);
        out("\nHAL ABI: ");
        number(h->abi);
        out("\nSector bytes: ");
        number(h->block.sector_bytes);
        out("\nSectors: ");
        number(h->block.sectors);
        out("\nBlock write capability: ");
        out(h->block.writable ? "yes" : "no");
        putch('\n');
        return 0;
    }
    if (same(c, "osk")) {
        if (argc == 1) {
            out(multi_osk_enabled() ? "OSK ON\n" : "OSK OFF\n");
            return 0;
        }
        if (argc != 2)
            return INVALID;
        if (same(v[1], "on"))
            return multi_osk_enable(1);
        if (same(v[1], "off"))
            return multi_osk_enable(0);
        if (same(v[1], "toggle"))
            return multi_osk_enable(!multi_osk_enabled());
        return INVALID;
    }
    if (same(c, "modules")) {
        multi_modules_list();
        return 0;
    }
    if (same(c, "drivers")) {
        const char *d = multi_hal()->drivers;
        out(d ? d : "Platform did not publish a driver inventory.\n");
        return 0;
    }
    if (same(c, "poweroff") || same(c, "shutdown") || same(c, "reboot")) {
        if (argc != 1)
            return INVALID;
        TRY(disk_flush());
        const MultiHal *h = multi_hal();
        if (same(c, "reboot"))
            return h->reboot ? h->reboot(h->lifecycle_ctx) : UNSUP;
        return h->poweroff ? h->poweroff(h->lifecycle_ctx) : UNSUP;
    }
    if (same(c, "cls") || same(c, "clear")) {
        clear_screen();
        return 0;
    }
    if (same(c, "echo")) {
        for (int i = 1; i < argc; i++) {
            if (i > 1)
                putch(' ');
            out(v[i]);
        }
        nl();
        return 0;
    }
    if (same(c, "history")) {
        for(u32 i=0;i<history_count;i++){number(i+1);out(" ");out(history[(history_next+HISTORY_COUNT-history_count+i)%HISTORY_COUNT]);nl();}
        return 0;
    }
    if (same(c, "exit")) {
        TRY(disk_flush());
        running = 0;
        return 0;
    }
    if (same(c, "pwd")) {
        out(cwd);
        nl();
        return 0;
    }
    if (same(c,"fs")) {
        if(argc==2&&same(v[1],"drivers")){fs_list_drivers();return 0;}
        if(argc!=1)return INVALID;
        if(!vol.mounted){out("No filesystem mounted\n");return 0;}
        out(fs_name());out((fs_capabilities()&MULTIOS_FS_WRITE)?" | experimental writes; read-only by default\n":" | READ-ONLY backend; no write callbacks\n");
        out("Partition base LBA: ");number(vol.base);out("\nVolume sectors: ");number(vol.sectors);nl();
#ifdef MULTIOS_STORAGE_TX
        out(multi_storage_description());nl();
#endif
        return 0;
    }
    if(same(c,"mount")) {
        if(argc!=2)return INVALID;
        int which=-1;
        if(same(v[1],"auto"))which=0;else if(same(v[1],"raw"))which=5;
        else if(v[1][0]>='1'&&v[1][0]<='4'&&!v[1][1])which=v[1][0]-'0';
        if(which<0)return INVALID;
        int mr=fs_mount_select(which);cwd[0]='/';cwd[1]=0;if(mr<0)return mr;
        out(fs_name());out(" mounted read-only at /\n");return 0;
    }
    if (same(c, "lock")) {
        TRY(disk_flush());
        vol.writable = 0;
        out("Read-only\n");
        return 0;
    }
    if (same(c, "unlock")) {
        if (argc != 2 || !streq(v[1], "YES"))
            return INVALID;
        TRY(fs_unlock());
        out("EXPERIMENTAL WRITE ENABLED. Disposable images only!\n");
        return 0;
    }
    if (same(c, "sync")) {
        TRY(disk_flush());
        out("Flushed (not a filesystem repair)\n");
        return 0;
    }
    if (!vol.mounted)
        return BAD;
    if (same(c, "df")) {
        out("Filesystem: ");out(fs_name());nl();
        u32 free;
        TRY(fs_free(&free));
        out("Cluster bytes: ");
        number(vol.cluster_bytes);
        out("\nTotal clusters: ");
        number(vol.clusters);
        out("\nFree clusters: ");
        number(free);
        out("\nFree KiB: ");
        number64((uint64_t)free * vol.cluster_bytes / 1024);
#ifdef MULTIOS_STORAGE_TX
        nl();out(multi_storage_description());
#endif
        out("\nMode: ");
        out(vol.writable ? "RW" : "RO");
        out(" | Dirty: ");
        number(!!(vol.flags & 2));
        nl();
        return 0;
    }
    if (same(c, "dir") || same(c, "ls") || same(c, "tree")) {
        if (argc > 2)
            return INVALID;
        return list(argc == 2 ? v[1] : cwd, same(c, "tree"), 0);
    }
    if (same(c, "cd")) {
        if (argc == 1) {
            out(cwd);
            nl();
            return 0;
        }
        if (argc != 2)
            return INVALID;
        TRY(get(v[1], p, &e));
        if (!(e.attr & ATTR_DIR))
            return NOTDIR;
        mcopy(cwd, p, slen(p) + 1);
        return 0;
    }
    if (same(c, "type") || same(c, "cat") || same(c, "hex")) {
        if (argc != 2)
            return INVALID;
        return dump(v[1], same(c, "hex"));
    }
    if(same(c,"more")||same(c,"source")){if(argc!=2)return INVALID;return same(c,"more")?more(v[1]):source(v[1]);}
    if (same(c, "crc32")) {
        if (argc != 2)
            return INVALID;
        return crc(v[1]);
    }
    if (same(c, "cmp")) {
        if (argc != 3)
            return INVALID;
        return compare(v[1], v[2]);
    }
    if (same(c, "find")) {
        if (argc != 3)
            return INVALID;
        return findtext(v[1], v[2]);
    }
    if(same(c,"stat64")||same(c,"readat")||same(c,"hexat"))return wide_command(argc,v);
    if(same(c,"writeat")||same(c,"resize")){
        int writing=same(c,"writeat");if(argc!=(writing?4:3))return INVALID;
        uint64_t off;TRY(unsigned64(v[2],&off));TRY(pathof(v[1],p));
        return writing?fs_write64(p,off,(const u8*)v[3],slen(v[3])):fs_resize64(p,off);
    }
#ifdef MULTIOS_BIOS_FAULT_TEST
    if(same(c,"testraw")){extern u32 multi_bios_test_writes(void);out("TEST raw writes=");number(multi_bios_test_writes());nl();return 0;}
#endif
    if(same(c,"streams")){
        if(argc!=2)return INVALID;TRY(pathof(v[1],p));u32 cursor=0;MultiNamedStream e;int r;
        while((r=fs_stream_next(p,&cursor,&e))>0){out(e.name);out(" | bytes=");number64(e.size);out(" valid=");number64(e.valid);out(" flags=0x");hexnum(e.flags,4);nl();}
        return r;
    }
    if(same(c,"sread")||same(c,"shex")){
        if(argc!=5)return INVALID;TRY(pathof(v[1],p));uint64_t off,n;TRY(unsigned64(v[3],&off));TRY(unsigned64(v[4],&n));if(n>4096)return INVALID;
        u8 buf[256];for(uint64_t done=0;done<n;){u32 amount=n-done>sizeof buf?sizeof buf:(u32)(n-done);
            if(done>UINT64_MAX-off)return INVALID;TRY(fs_stream_read(p,v[2],off+done,buf,amount));
            for(u32 i=0;i<amount;i++){if(same(c,"shex")){hexnum(buf[i],2);putch(' ');}else putch(buf[i]);}done+=amount;}
        nl();return 0;
    }
    if(same(c,"snew")||same(c,"swrite")||same(c,"sappend")||same(c,"swriteat")||same(c,"sresize")||same(c,"sdel")){
        int op=same(c,"snew")?MULTIOS_STREAM_CREATE:same(c,"swrite")?MULTIOS_STREAM_REPLACE:same(c,"sappend")?MULTIOS_STREAM_APPEND:same(c,"swriteat")?MULTIOS_STREAM_WRITE:same(c,"sresize")?MULTIOS_STREAM_RESIZE:MULTIOS_STREAM_REMOVE;
        int args=op==MULTIOS_STREAM_WRITE?5:op==MULTIOS_STREAM_REMOVE?3:4;if(argc!=args)return INVALID;
        TRY(pathof(v[1],p));uint64_t off=0;const u8*b=0;u32 n=0;
        if(op==MULTIOS_STREAM_WRITE||op==MULTIOS_STREAM_RESIZE)TRY(unsigned64(v[3],&off));
        if(op<=MULTIOS_STREAM_WRITE){b=(const u8*)v[op==MULTIOS_STREAM_WRITE?4:3];n=slen((const char*)b);}
        return fs_stream_change(p,v[2],op,off,b,n);
    }
    if(same(c,"copyads")||same(c,"copyadsresume")){
        int resume=same(c,"copyadsresume");if(argc!=(resume?4:3))return INVALID;
        char dest[PATHMAX],stage[PATHMAX];TRY(pathof(v[1],p));TRY(pathof(v[2],dest));if(resume)TRY(pathof(v[3],stage));
        MultiCopyResult result;int r=fs_copy_streams(p,dest,resume?stage:0,&result,0,0);
        if(r<0&&result.staging[0]){out("Staging retained: ");out(result.staging);nl();}
        if(!r)out("Copied and verified DATA + ADS (not ACL metadata).\n");return r;
    }
    if(same(c,"link")){if(argc!=3)return INVALID;char destination[PATHMAX];TRY(pathof(v[1],p));TRY(pathof(v[2],destination));return fs_link(p,destination);}
    if (same(c, "stat")) {
        if (argc != 2)
            return INVALID;
        TRY(get(v[1], p, &e));
        out("Path: ");
        out(p);
        out("\nSize: ");
        number(e.data.size);
        out("\nValid data: ");
        number(e.data.valid);
        out(vol.kind==FS_NTFS?"\nMFT record: ":"\nFirst cluster: ");
        number(e.data.first);
        out("\nAttributes: 0x");
        hexnum(e.attr, 4);
        if(vol.kind==FS_EXFAT){out("\nNoFatChain: ");number(!!(e.data.flags & 2));}
        nl();
        return 0;
    }
    if (same(c, "ren")) {
        if (argc != 3)
            return INVALID;
        TRY(pathof(v[1], p));
        if (isancestor(p, cwd))
            return INVALID;
        return fs_rename(p, v[2]);
    }
    if (same(c, "del") || same(c, "rm") || same(c, "rd") || same(c, "rmdir")) {
        if (argc != 2)
            return INVALID;
        TRY(pathof(v[1], p));
        if (isancestor(p, cwd))
            return INVALID;
        return fs_remove(p, same(c, "rd") || same(c, "rmdir"));
    }
    if (same(c, "copy") || same(c, "cp") || same(c, "copyatomic") || same(c, "copyresume") || same(c,"copypause")) {
        int resume = same(c, "copyresume"), pause = same(c,"copypause");
        if (argc != (resume || pause ? 4 : 3))
            return INVALID;
        TRY(pathof(v[1], p));
        TRY(pathof(v[2], q));
#ifdef MULTIOS_STORAGE_TX
        if (!same(c, "copyatomic")) {
            char stage[PATHMAX];
            if (resume) { TRY(pathof(v[3], stage)); }
            uint64_t limit = 0;
            if (pause) { TRY(unsigned64(v[3], &limit)); if(!limit)return INVALID; }
            MultiCopyResult result;
            int r = fs_copy_staged(p, q, resume ? stage : 0, &result,
                                   pause ? pause_copy : 0, &limit);
            if (r < 0 && result.staging[0]) {
                out("Staging retained (recover/remount first after I/O error): ");
                out(result.staging); out("\n");
            } else if (!r) {
                out("Copied and verified "); number64(result.total); out(" bytes.\n");
            }
            return r;
        }
#else
        if (resume || pause) return UNSUP;
#endif
        return fs_put(q, 0, 0, 4, p);
    }
    if (same(c, "md") || same(c, "mkdir") || same(c, "touch")) {
        if (argc != 2)
            return INVALID;
        TRY(pathof(v[1], p));
        return fs_put(p, 0, 0, same(c, "touch") ? 0 : 3, 0);
    }
    if (same(c, "write") || same(c, "append")) {
        if (argc != 3)
            return INVALID;
        TRY(pathof(v[1], p));
        u8 data[512];
        u32 n = slen(v[2]);
        mcopy(data, v[2], n);
        data[n++] = '\n';
        return fs_put(p, data, n, same(c, "write") ? 1 : 2, 0);
    }
    out("Unknown command. Type HELP.\n");
    return INVALID;
}
void shell(void) {
    out("\nMultiOS " MULTIOS_VERSION " | MultiPulti core\n" MULTIOS_CREDIT "\n" MULTIOS_STATUS "\nDOS-like console | exFAT / FAT32 / NTFS | READ-ONLY by default\n");
    int r = fs_mount();
    if (r < 0) {
        out("Mount failed: ");
        out(errstr(r));
        nl();
    } else {
        out(fs_name());out(" mounted at / (A: concept; paths use /)\n");
        if (vol.flags & 2)
            out("WARNING: dirty volume; writes forbidden. Check with the filesystem-specific external tool.\n");
    }
    out("HELP for commands. EXIT to stop.\n");
    while (running) {
        out("MultiOS:");
        out(cwd);
        out(vol.writable ? "> " : " [RO]> ");
        MultiEdit edit;multi_edit_reset(&edit);char draft[512]="";u32 browsing=history_count;
        while(1){
            int ch=input_char();if(ch<0){running=0;break;}
            u32 oldlen=columns(edit.text,edit.length),oldpos=columns(edit.text,edit.cursor);
            if(ch==16||ch==14||ch==MULTI_KEY_HISTORY_PREV||ch==MULTI_KEY_HISTORY_NEXT){
                if(browsing==history_count)mcopy(draft,edit.text,edit.length+1);
                if(ch==16||ch==MULTI_KEY_HISTORY_PREV){if(browsing)browsing--;}
                else if(browsing<history_count)browsing++;
                multi_edit_load(&edit,browsing==history_count?draft:history[(history_next+HISTORY_COUNT-history_count+browsing)%HISTORY_COUNT]);
                edit_paint(&edit,oldlen,oldpos,MULTI_EDIT_CHANGED);continue;
            }
            int flags=multi_edit_key(&edit,ch);
            if(flags&MULTI_EDIT_SUBMIT){for(u32 i=edit.cursor;i<edit.length;i++)putch(edit.text[i]);nl();break;}
            if(flags&MULTI_EDIT_CANCEL){out("^C\n");break;}
            edit_paint(&edit,oldlen,oldpos,flags);
        }
        if(!running)break;
        u32 n=edit.length;mcopy(line,edit.text,n+1);
        if(edit.overflow){out("Error: line too long or invalid/incomplete UTF-8; not executed\n");continue;}
        if (!n)
            continue;
        char saved[512];
        mcopy(saved, line, n + 1);
        char *v[16];
        int argc = tokenize(line, v);
        if (argc < 0) {
            out("Error: unmatched quote or too many arguments\n");
            continue;
        }
        if (!argc)
            continue;
        r = execute(argc, v);
        if (r < 0) {
            out("Error: ");
            out(errstr(r));
            nl();
        }
        if (!same(v[0], "history")){mcopy(last, saved, n + 1);history_add(saved);}
    }
    out("MultiOS stopped.\n");
    platform_exit();
}
