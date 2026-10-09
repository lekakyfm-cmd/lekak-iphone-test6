/* Execute the actual upstream roster/JSON implementation with the supplied
 * Lekak manifest. Upstream fixture provides only non-roster services:
 * portraits, text rendering, cards and storage paths remain test stubs.
 */
#define main UpstreamRosterTests
#include "../EngineSDK/tests/pc/duelists_test.c"
#undef main
#include "pc/mods/modapi.h"
#include "game/campaign_flags.h"
extern int LekakLegacyInit(const MemoriesModHost *,MemoriesMod *);
static int gate,notifications,subscriptions;
static uint64_t now;
static MemoriesModCallback callbacks[2];
static const char *last_message;
void Library_UpdateCardUsedFlag(s32 flag) {
 assert((flag&~CAMPAIGN_FLAG_CLEAR_MODIFIER)==1806);
 gate=!(flag&CAMPAIGN_FLAG_CLEAR_MODIFIER);story_flag=gate?1806:-1;
}
static int resolve(const MemoriesModHost *h,const char *id) {(void)h;return Duelists_Find(id);}
static uint64_t clock_now(const MemoriesModHost *h) {(void)h;return now;}
static int subscribe(const MemoriesModHost *h,unsigned event,int priority,MemoriesModCallback callback) {
 (void)h;(void)priority;assert(event==MEMORIES_EVENT_LOAD||event==MEMORIES_EVENT_SLOT_LOAD);
 assert(subscriptions<2);callbacks[subscriptions++]=callback;return subscriptions;
}
static void unsubscribe(const MemoriesModHost *h,int token) {(void)h;(void)token;}
static void size(const MemoriesModHost *h,int *width,int *height,int *scale) {(void)h;*width=640;*height=480;*scale=1;}
static int text_width(const MemoriesModHost *h,const char *text,int scale) {(void)h;return (int)strlen(text)*8*scale;}
static void fill(const MemoriesModHost *h,int x,int y,int w,int height,uint32_t rgb,unsigned alpha) {
 (void)h;(void)x;(void)y;(void)rgb;(void)alpha;assert(w>0&&height>0);
}
static void draw(const MemoriesModHost *h,int x,int middle,const char *text,uint32_t rgb,int scale) {
 (void)h;(void)x;(void)middle;(void)rgb;assert(scale>0);notifications++;last_message=text;
}
static void set_wins(const char *identity,unsigned count) {
 int id=Duelists_Named(identity);assert(id>0);Duelists_RecordSlot(gDuel_awPlayerDeck,id)[0]=count;
}
int main(int argc,char **argv) {
 assert(argc==2);assert(!UpstreamRosterTests());
 build(NULL,NULL);ids[0]="Lekakmod";char error[256];
 manifests[0]=Json_ParseFile(argv[1],error,sizeof(error));assert(manifests[0]);mod_count=1;
 memset(gDuel_awPlayerDeck,0,sizeof(gDuel_awPlayerDeck));gFreeDuel_nExtraOwner=0;story_flag=-1;
 for(int i=0;i<9;i++)gDuel_aOpponentData[38][i]=(signed char)(20+i);
 Duelists_Clear();Duelists_Build();assert(Duelists_Count()==80);
 const JsonValue *entries=Json_Member(Json_Root(manifests[0]),"duelists");
 assert(Json_Count(entries)==40);
 for(int i=0;i<40;i++) {
  const JsonValue *entry=Json_At(entries,i);char identity[128];
  snprintf(identity,sizeof(identity),"Lekakmod:%s",Json_String(Json_Member(entry,"id"),""));
  int id=Duelists_Find(identity);assert(id==40+i);assert(Duelists_BaseId(id)==38);
  assert(!memcmp(Duelists_AiRow(id),gDuel_aOpponentData[38],9));
 }
 assert(Duelists_Find("Lekakmod:lesub")==77);
 assert(Duelists_Find("Lekakmod:yem")==78);
 assert(Duelists_Find("Lekakmod:lekak")==79);
 MemoriesModHost host={0};host.api=MEMORIES_MOD_API;host.duelist_id=resolve;host.now_us=clock_now;
 host.subscribe=subscribe;host.unsubscribe=unsubscribe;host.overlay_size=size;
 host.text_width=text_width;host.draw_text=draw;host.fill=fill;
 MemoriesMod mod={0};assert(LekakLegacyInit(&host,&mod));
 mod.frame();mod.overlay();assert(!gate && !notifications);
 set_wins("Heishin",1);mod.frame();mod.overlay();assert(!gate && notifications==1);
 assert(!strcmp(last_message,"DARK SIMON UNLOCKED!"));
 now=4000001;mod.frame();mod.overlay();assert(notifications==1);
 set_wins("Pegasus",100);set_wins("Lekakmod:dark-simon",5);set_wins("Nitemare",1);
 cards_named=82;((unsigned char *)gDuel_awPlayerDeck)[0x50+82]=1;
 mod.frame();assert(gate);
 assert(Duelists_Unlocked(gDuel_awPlayerDeck,Duelists_Find("Lekakmod:tyrant-kaiba")));
 /* A different empty save must clear flag 1806 and the queued old notices. */
 memset(gDuel_awPlayerDeck,0,sizeof(gDuel_awPlayerDeck));
 memcpy((unsigned char *)gDuel_awPlayerDeck+0x334,"ABCD",4);
 Duelists_Frame();notifications=0;mod.frame();mod.overlay();assert(!gate && !notifications);
 /* Reload event resets the notification baseline, even for the same code. */
 MemoriesModEvent event={0};event.phase=MEMORIES_AFTER;callbacks[0](&event);
 mod.frame();mod.overlay();assert(!notifications);
 Duelists_Clear();Json_Free(manifests[0]);mod_count=0;
 puts("Lekak manifest: real SDK roster, all 40 Nitemare AI copies, ordering, native legacy gate and save-switch notification tests passed");
 return 0;
}
