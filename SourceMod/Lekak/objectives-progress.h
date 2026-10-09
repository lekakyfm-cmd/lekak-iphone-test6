#include <stdio.h>
#include <string.h>
#include "pc/cards/cards.h"
#include "ygo_types.h"
#include "game/text_encode_decimal_digits.h"
#include "objectives-progress-data.h"
/* Each of the 80 templates is compiled at most once. Numbers and colour
   bytes are refreshed when a page opens; navigation allocates no new text. */
typedef struct {u8 *bytes;size_t size;int number[6],colour[6];} ProgressTemplate;
static ProgressTemplate progress_templates[2][40];
static int progress_card(const char *ref){return Cards_Named(ref);}
static int progress_value(const ProgressCondition *c){
 int id,count=0;
 switch(c->kind){
 case 0:id=Duelists_Named(c->ref);return id>0?Duelists_RecordSlot(gDuel_awPlayerDeck,id)[0]:0;
 case 1:for(id=1;id<Duelists_Count();id++)count+=Duelists_RecordSlot(gDuel_awPlayerDeck,id)[0];return count;
 case 2:id=Duelists_Named(c->ref);return id>0&&Duelists_Unlocked(gDuel_awPlayerDeck,id);
 case 3:id=progress_card(c->ref);return id>0&&Cards_Valid(id)&&Cards_Seen(id);
 case 4:id=progress_card(c->ref);if(id<=0||!Cards_Valid(id))return 0;count=*Cards_ChestSlot(gDuel_awPlayerDeck,id);for(int k=0;k<40;k++)count+=gDuel_awPlayerDeck[k]==id;return count;
 case 5:for(int k=0;k<(strcmp(c->ref,"first6")==0?6:37);k++){id=Duelists_Named(objectives[k].identity);count+=id>0&&Duelists_Unlocked(gDuel_awPlayerDeck,id);}return count;
 case 6:if(strcmp(c->ref,"retail39")==0){for(id=1;id<40;id++)count+=Duelists_RecordSlot(gDuel_awPlayerDeck,id)[0]>=1;}
  else for(int k=0;k<37;k++){id=Duelists_Named(objectives[k].identity);count+=id>0&&Duelists_RecordSlot(gDuel_awPlayerDeck,id)[0]>=1;}return count;
 case 7:for(id=1;id<=gCard_nCount;id++)count+=Cards_Valid(id)&&Cards_Seen(id);return count;
 }
 return 0;
}
static u8 *progress_text(void){
 ProgressTemplate *t=&progress_templates[english][page];
 int n=progress_counts[page];
 if(!t->bytes){
  char listing[1600],label[160],card[100];size_t used=0;
  used+=snprintf(listing+used,sizeof(listing)-used,"@bank dialog\n[FE6A]\n");
  for(int k=0;k<n;k++){
   const ProgressCondition *c=&progress_conditions[page][k];
   const char *title=english?c->en:c->fr;
   if(strcmp(title,"@card")==0){
    int id=progress_card(c->ref);if(!Cards_NameUtf8(id,card,sizeof(card)))snprintf(card,sizeof(card),english?"Card #%d":"Carte n°%d",id);
    snprintf(label,sizeof(label),"%s%s\n%s",card,id==443?" (#443)":"",english?"Found in Library":"Dans la bibliothèque");title=label;
   }
   used+=snprintf(listing+used,sizeof(listing)-used,"%s{f8 0A 00}%s : 00000/%d",k?"\n":"",title,c->target);
   if(used>=sizeof(listing)-16)return NULL;
  }
  snprintf(listing+used,sizeof(listing)-used,"{end}");
  size_t size=0;u8 *bytes=(u8 *)Text_CompileOwn(listing,0xFE6A,&size);if(!bytes)return NULL;
  u8 digits[5];Text_EncodeDecimalDigits(10000,5,digits);int z=digits[4],number=0,colour=0;
  for(size_t i=0;i<size;i++){
   if(i+2<size&&bytes[i]==0xF8&&bytes[i+1]==0x0A&&colour<n)t->colour[colour++]=i+2;
   if(i+4<size&&bytes[i]==z&&bytes[i+1]==z&&bytes[i+2]==z&&bytes[i+3]==z&&bytes[i+4]==z&&number<n){t->number[number++]=i;i+=4;}
  }
  if(number!=n||colour!=n)return NULL;
  t->bytes=bytes;t->size=size;
 }
 for(int k=0;k<n;k++){
  const ProgressCondition *c=&progress_conditions[page][k];int value=progress_value(c);
  /* Completed progress stops at its goal; avoid overflow past five digits. */
  if(value>c->target)value=c->target;
  Text_EncodeDecimalDigits(value,5,t->bytes+t->number[k]);
  if(!value){u8 digits[5];Text_EncodeDecimalDigits(10000,5,digits);t->bytes[t->number[k]+4]=digits[4];}
  t->bytes[t->colour[k]]=value>=c->target?1:0;
 }
 return t->bytes;
}
static void progress_box(int fallback){
 u8 *text=progress_text();
 if(!text){box(0,fallback,16,60,288,136);return;}
 u8 *destination=(u8 *)Text_Own(0xFE6A);
 size_t size=progress_templates[english][page].size;
 if(!destination||size>1024){box(0,fallback,16,60,288,136);return;}
 remove_box(0);
 /* The first build resolves field_36 itself. Supply our reserved listing
    before creation, rather than setting a cursor initialization overwrites. */
 memcpy(destination,text,size);
 boxes[0]=TextBox_CreateFlagged(0,0xFE6A,16,60,288,136,0x1028);
 if(boxes[0])func_80039A60(boxes[0]);
}
