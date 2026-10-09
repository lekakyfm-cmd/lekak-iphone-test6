/* Source replacement for the supplied i386-only legacy notification object.
 * The first-six gate (flag 1806) is recovered from sync_kaiba disassembly.
 * Uses the actual SDK, not its old 32-bit vtable byte offsets.
 * Guest accesses still require the upstream LLVM translation before iOS use.
 */
#include "pc/mods/modapi.h"
#include "pc/free_duel/duelists.h"
#include "game/campaign_flags.h"
#include <string.h>
extern unsigned short gDuel_awPlayerDeck[];
static const MemoriesModHost *host;
static const char *const identities[7]={
 "Lekakmod:dark-simon","Lekakmod:destroyer-pegasus","Lekakmod:soul-reaper-bakura",
 "Lekakmod:eclipse-isis","Lekakmod:primal-rex","Lekakmod:heishin-the-eternal","Lekakmod:tyrant-kaiba"
};
static const char *const messages[7]={
 "DARK SIMON UNLOCKED!","DESTROYER PEGASUS UNLOCKED!","SOUL REAPER BAKURA UNLOCKED!",
 "ECLIPSE ISIS UNLOCKED!","PRIMAL REX UNLOCKED!","HEISHIN THE ETERNAL UNLOCKED!","TYRANT KAIBA UNLOCKED!"
};
static unsigned save_identity;
static int ready,before[7],queue[14],count,active=-1;
static uint64_t until;
static void clear(void) {ready=0;count=0;active=-1;until=0;memset(before,0,sizeof(before));}
static void sync_kaiba(void) {
 int satisfied=1;
 for(int i=0;i<6;i++) {
  int id=host->duelist_id(host,identities[i]);
  if(id<=0 || !Duelists_Unlocked(gDuel_awPlayerDeck,id))satisfied=0;
 }
 Library_UpdateCardUsedFlag(1806|(satisfied?0:CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
static void frame(void) {
 unsigned current;
 memcpy(&current,(unsigned char *)gDuel_awPlayerDeck+0x334,sizeof(current));
 if(ready && current!=save_identity)clear();
 sync_kaiba();
 for(int i=0;i<7;i++) {
  int id=host->duelist_id(host,identities[i]);
  int available=id>0 && Duelists_Unlocked(gDuel_awPlayerDeck,id);
  if(ready && available && !before[i] && count<14)queue[count++]=i;
  before[i]=available;
 }
 ready=1;save_identity=current;
 uint64_t now=host->now_us(host);
 if(active>=0 && now>=until)active=-1;
 if(active<0 && count) {
  active=queue[0];memmove(queue,queue+1,(size_t)(--count)*sizeof(queue[0]));
  until=now+4000000;
 }
}
static void overlay(void) {
 if(active<0)return;
 int width,height,scale;host->overlay_size(host,&width,&height,&scale);
 scale=scale>0?scale*2:2;
 while(scale>1 && host->text_width(host,messages[active],scale)+40*scale>width)scale--;
 int text=host->text_width(host,messages[active],scale),x=(width-text)/2,middle=height/5;
 host->fill(host,x-20*scale,middle-22*scale,text+40*scale,44*scale,0x160D22,235);
 host->fill(host,x-20*scale,middle-22*scale,text+40*scale,2*scale,0xB73C54,255);
 host->draw_text(host,x,middle,messages[active],0xFFE1A6,scale);
}
static void reset(void) {clear();sync_kaiba();}
static void applied(int on) {clear();if(on)sync_kaiba();}
static void loaded(MemoriesModEvent *event) {if(event->phase==MEMORIES_AFTER)clear();}
int LekakLegacyInit(const MemoriesModHost *h,MemoriesMod *mod) {
 if(!h || !mod || h->api<5 || !h->duelist_id || !h->subscribe || !h->unsubscribe ||
    !h->now_us || !h->overlay_size || !h->text_width || !h->fill || !h->draw_text)return 0;
 host=h;
 int first=h->subscribe(h,MEMORIES_EVENT_LOAD,-1,loaded);
 if(!first)return 0;
 if(!h->subscribe(h,MEMORIES_EVENT_SLOT_LOAD,-1,loaded)){h->unsubscribe(h,first);host=NULL;return 0;}
 mod->api=MEMORIES_MOD_API;
 clear();mod->frame=frame;mod->reset=reset;mod->applied=applied;mod->overlay=overlay;mod->overlay_signature=NULL;
 return 1;
}
