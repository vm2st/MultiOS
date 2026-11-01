/* MultiPulti | vm2 studios | t.me/v2vproj — own HID boot keyboard decoder. */
#include "multios/hid.h"
void multi_hid_reset(MultiHid *h){if(h)mzero(h,sizeof(*h));}
static int translated(MultiHid *h,u8 k,u8 mod){
 int shift=!!(mod&0x22),ctrl=!!(mod&0x11);
 if(k>=4&&k<=29){int c='a'+k-4;if(ctrl)return c-'a'+1;return (shift^h->caps)?c-32:c;}
 if(k>=30&&k<=39)return (shift?"!@#$%^&*()":"1234567890")[k-30];
 if(k>=45&&k<=56)return (shift?"_+{}|~:\"~<>?":"-=[]\\#;'`,./")[k-45];
 switch(k){
 case 40:case 88:return '\n';case 41:return 27;case 42:return 8;case 43:return '\t';case 44:return ' ';
 case 59:return MULTI_KEY_OSK;case 62:return MULTI_KEY_PREVIOUS;case 63:return MULTI_KEY_NEXT;case 64:return MULTI_KEY_SELECT;
 case 65:return MULTI_KEY_HISTORY_PREV;case 66:return MULTI_KEY_HISTORY_NEXT;
 case 74:return MULTI_KEY_HOME;case 77:return MULTI_KEY_END;case 76:return MULTI_KEY_DELETE;
 case 79:return MULTI_KEY_RIGHT;case 80:return MULTI_KEY_LEFT;case 81:return MULTI_KEY_DOWN;case 82:return MULTI_KEY_UP;
 case 84:return '/';case 85:return '*';case 86:return '-';case 87:return '+';case 100:return shift?'|':'\\';
 }
 if(k>=89&&k<=99){if(h->num)return "1234567890."[k-89];switch(k){case 90:return MULTI_KEY_DOWN;case 92:return MULTI_KEY_LEFT;case 94:return MULTI_KEY_RIGHT;case 96:return MULTI_KEY_UP;}}
 return 0;
}
int multi_hid_report(MultiHid *h,const u8 *r,u32 length){
 if(!h||!r||length!=8)return INVALID;
 for(int i=2;i<8;i++)if(r[i]>=1&&r[i]<=3){h->rollovers++;return UNSUP;}
 for(int i=2;i<8;i++){
  u8 k=r[i];if(!k)continue;int seen=0;
  for(int j=2;j<i;j++)if(r[j]==k)seen=1;
  for(int j=2;j<8;j++)if(h->previous[j]==k)seen=1;
  if(seen)continue;
  if(k==57){h->caps^=1;continue;}if(k==83){h->num^=1;continue;}
  int event=translated(h,k,r[0]);if(!event)continue;
  u8 next=(h->head+1)%MULTI_HID_QUEUE;
  if(next==h->tail){h->overflows++;continue;}
  h->queue[h->head]=event;h->head=next;
 }
 mcopy(h->previous,r,8);return 0;
}
int multi_hid_get(MultiHid *h){if(!h||h->tail==h->head)return -2;int e=h->queue[h->tail];h->tail=(h->tail+1)%MULTI_HID_QUEUE;return e;}
