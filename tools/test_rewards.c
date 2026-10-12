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
 puts("Rewards: retroactive wins, once-only payouts, save/load, caps, card grant, TEC, corruption and new saves passed.");
 return 0;
}
