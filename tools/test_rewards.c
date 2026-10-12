/* Exercises the actual iPhone reward module with deterministic host tables. */
#include <assert.h>
#include <string.h>
#include <stdio.h>
/* Host-only fixture: guest pointer annotations require the pinned LLVM
 * compiler, while macOS cc uses Apple Clang. No guest layouts are exercised
 * by this fixture; actual module compilation keeps the real G32 annotations. */
#include "../EngineSDK/src/types.h"
#undef G32
#define G32
#define MEMORIES_DECOMP_DISPLAY_OBJECT_H
#define MEMORIES_DECOMP_DUEL_RESULT_DISPLAY_H
typedef struct {unsigned flags;} DisplayObject;
#define DISPLAY_OBJECT_FLAG_RENDERABLE 1
typedef struct {unsigned char is_tec_rank,rank_tier;} DuelResultDisplayState;
extern DuelResultDisplayState *D_8009B1E8;
#include "../SourceMod/Lekak/rewards.c"
static unsigned short records[ROSTER_COUNT+1][2];
static unsigned char chest[2048];
static unsigned seen;
int Duelists_Named(const char *name){for(int i=0;i<ROSTER_COUNT;i++)if(!strcmp(name,roster[i].ref))return i+1;return 0;}
unsigned short *Duelists_RecordSlot(void *state,int id){(void)state;assert(id>0&&id<=ROSTER_COUNT);return records[id];}
unsigned int Tables_StarchipCap(void){return 999999;}
int Tables_ChestRoom(void){return 255;}
int Cards_Named(const char *name){assert(name&&*name);return 123;}
int Cards_Valid(int id){return id>0&&id<2048;}
unsigned char *Cards_ChestSlot(void *state,int id){(void)state;return &chest[id];}
void Cards_MarkSeen(int id){assert(id==123);seen++;}
static void stock_build(void *p){memset(p,0,0xf00);memcpy((unsigned char *)p+0x200,save_base,0x680);memcpy((unsigned char *)p+0x880,save_base,0x680);memset((unsigned char *)p+0x828,0,88);memset((unsigned char *)p+0xea8,0,88);memset(save_base+TAIL_OFFSET,0,88);}
static void stock_apply(void *p){memcpy(save_base,p,0x680);}
static int screen_w,screen_h,drawn,last_middle,last_scale;
static void screen(const MemoriesModHost *h,int *w,int *hh,int *s){(void)h;*w=screen_w;*hh=screen_h;*s=1;}
static int measure(const MemoriesModHost *h,const char *v,int s){(void)h;return (int)strlen(v)*12*s;}
static void paint(const MemoriesModHost *h,int x,int middle,const char *v,uint32_t rgb,int s){
 (void)h;(void)rgb;if(!*v)return;
 assert(x>=0&&x+measure(h,v,s)<=screen_w);
 assert(middle-7*s>=0&&middle+7*s<=screen_h);
 if(drawn)assert(middle-7*s>=last_middle+7*last_scale);
 last_middle=middle;last_scale=s;drawn++;
}
static void rect(const MemoriesModHost *h,int x,int y,int w,int hh,uint32_t rgb,unsigned alpha){(void)h;(void)rgb;(void)alpha;assert(x>=0&&y>=0&&x+w<=screen_w&&y+hh<=screen_h);}
static void check_layout(void){
 static MemoriesModHost ui;ui.overlay_size=screen;ui.text_width=measure;ui.draw_text=paint;ui.fill=rect;host=&ui;opened=1;
 const int sizes[][2]={{320,240},{512,240},{640,480},{1280,720}};
 for(unsigned j=0;j<sizeof(sizes)/sizeof(sizes[0]);j++){
  screen_w=sizes[j][0];screen_h=sizes[j][1];
  for(int lang=0;lang<2;lang++)for(int cat=0;cat<CATEGORY_COUNT;cat++){
   english=lang;category=cat;row=0;notice[0]=0;drawn=last_middle=last_scale=0;overlay();assert(drawn>=6);
  }
 }
 puts("Rewards layout: bitmap glyph bounds and line spacing passed at 320x240, 512x240, 640x480 and 1280x720, FR/EN.");
}
int main(void){
 unsigned char runtime[0x680]={0},payload[0xf00];unsigned chips=0;uint32_t code=77;
 save_base=runtime;starchips=&chips;memcpy(runtime+0x334,&code,4);
 old_build=stock_build;old_apply=stock_apply;category=AVAILABLE_CATEGORY;
 for(int i=1;i<=ROSTER_COUNT;i++)records[i][0]=50;
 available(ready());assert(available_count==330); /* Existing wins; historical TEC unavailable. */
 category=0;row=0;claim();assert(chips==20&&reward_claimed(ready(),0));
 claim();assert(chips==20); /* No duplicate payout. */
 build(payload);assert(reward_valid((RewardSave *)(payload+0x828),code));
 assert(!memcmp(payload+0x828,payload+0xea8,88));
 memset(runtime,0,sizeof(runtime));apply(payload+0x200);
 assert(reward_valid(ready(),code)&&reward_claimed(ready(),0));
 chips=999999;row=1;claim();assert(chips==999999&&!reward_claimed(ready(),1));
 chips=0;claim();assert(chips==100&&reward_claimed(ready(),1));
 reward_mark_tec(ready(),78);assert(reward_tec(ready(),78)&&reward_valid(ready(),code));
 category=ROSTER_COUNT;for(row=0;row<GROUP_COUNT;row++)if(groups[row].card)break;
 assert(row<GROUP_COUNT);int key=key_for(category,row);claim();assert(chest[123]==1&&seen==1&&reward_claimed(ready(),key));
 claim();assert(chest[123]==1&&seen==1);
 ready()->checksum^=1;assert(!ready()&&corrupt);
 code++;memcpy(runtime+0x334,&code,4);assert(reward_valid(ready(),code)&&!reward_claimed(ready(),0));
 assert(!reward_mark(ready(),-1)&&!reward_mark(ready(),512));
 check_layout();
 puts("Rewards: retroactive wins, once-only payouts, save/load, caps, card grant, TEC, corruption and new saves passed.");
 return 0;
}
