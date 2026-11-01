/* vm2 studios | t.me/v2vproj — MultiOS / MultiPulti */
#include "platform/uefi/platform.h"
#include "unicode.h"
static EFI_SYSTEM_TABLE *system_table;
static char pending[5];static u32 pending_at,pending_size;static u16 high;
static int getkey(void *ctx) {
    (void)ctx;
    if(pending_at<pending_size)return (u8)pending[pending_at++];
    for (;;) {
        EFI_INPUT_KEY key;
        EFI_STATUS status =
            uefi_call_wrapper(system_table->ConIn->ReadKeyStroke, 2, system_table->ConIn, &key);
        if (!EFI_ERROR(status)) {
            if(key.ScanCode)high=0;
            switch (key.ScanCode) {
            case 1:
                return MULTI_KEY_UP;
            case 2:
                return MULTI_KEY_DOWN;
            case 3:
                return MULTI_KEY_RIGHT;
            case 4:
                return MULTI_KEY_LEFT;
            case 5:return MULTI_KEY_HOME;
            case 6:return MULTI_KEY_END;
            case 8:return MULTI_KEY_DELETE;
            case 18:return MULTI_KEY_HISTORY_PREV; /* F8 */
            case 19:return MULTI_KEY_HISTORY_NEXT; /* F9 */
            case 15: /* F5: optional third button */
                return MULTI_KEY_PREVIOUS;
            case 16: /* F6 */
                return MULTI_KEY_NEXT;
            case 17: /* F7 */
                return MULTI_KEY_SELECT;
            case 12:
                return MULTI_KEY_OSK;
            case 23:
                return 27;
            }
            if(key.UnicodeChar){
                u16 c=key.UnicodeChar,units[2];u32 n=1;
                if(c>=0xd800&&c<=0xdbff){high=c;continue;}
                if(c>=0xdc00&&c<=0xdfff){if(!high)continue;units[0]=high;units[1]=c;n=2;}else units[0]=c;
                high=0;
                if(multi_utf16_to8(units,n,pending,sizeof(pending))<0)continue;
                pending_at=1;pending_size=slen(pending);return (u8)pending[0];
            }
        }
        EFI_EVENT event = system_table->ConIn->WaitForKey;
        UINTN index = 0;
        if (EFI_ERROR(uefi_call_wrapper(system_table->BootServices->WaitForEvent, 3, (UINTN)1,
                                        &event, &index)))
            uefi_call_wrapper(system_table->BootServices->Stall, 1, (UINTN)1000);
    }
}
static int open_keys(void *ctx, const char *options, MultiDevice *d) {
    if (*options) return INVALID;
    pending_at=pending_size=0;high=0;system_table=((MultiEfi *)ctx)->st;
    if (!system_table->ConIn) return UNSUP;
    d->input=(MultiInput){.getc=getkey}; return 0;
}
const MultiDriver multi_uefi_keys=MULTI_DRIVER("uefi-keys", MULTI_INPUT,
    "firmware SimpleTextInput key events", open_keys, 0);
