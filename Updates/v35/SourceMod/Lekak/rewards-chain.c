#include "pc/mods/modapi.h"
#include <string.h>
extern int LekakRewardsInit(const MemoriesModHost *,MemoriesMod *);
extern int LekakFusionInit(const MemoriesModHost *,MemoriesMod *);
extern int LekakBeforeRewardsInit(const MemoriesModHost *,MemoriesMod *);
static MemoriesMod previous,rewards;
static unsigned generation;
static void frame(void){if(previous.frame)previous.frame();if(rewards.frame)rewards.frame();}
static void applied(int on){if(previous.applied)previous.applied(on);if(rewards.applied)rewards.applied(on);}
static void reset(void){if(previous.reset)previous.reset();if(rewards.reset)rewards.reset();generation++;}
static void shutdown(void){if(rewards.shutdown)rewards.shutdown();if(previous.shutdown)previous.shutdown();}
static void overlay(void){if(previous.overlay)previous.overlay();if(rewards.overlay)rewards.overlay();}
static unsigned signature(void){
 if((previous.overlay&&!previous.overlay_signature)||(rewards.overlay&&!rewards.overlay_signature))return ++generation;
 unsigned a=previous.overlay_signature?previous.overlay_signature():0;
 unsigned b=rewards.overlay_signature?rewards.overlay_signature():0;
 return a*16777619U+b+generation;
}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m){
 int ok=LekakBeforeRewardsInit(h,m);if(!ok)return 0;
 previous=*m;memset(&rewards,0,sizeof(rewards));
 if(!LekakRewardsInit(h,&rewards)){
  if(h->log)h->log(h,"Rewards iPhone init failed; retaining original callbacks");
  return 1;
 }
 if(rewards.api>m->api)m->api=rewards.api;
 if(!LekakFusionInit(h,m)){
  if(h->log)h->log(h,"LEKAK iOS fusion protection unavailable; refuse unguarded fusion update");
  return 0;
 }
 m->frame=frame;m->applied=applied;m->reset=reset;m->shutdown=shutdown;
 m->overlay=overlay;m->overlay_signature=signature;
 if(h->log)h->log(h,"Lekak iPhone Rewards v1.1: 409 permanent goals initialized");
 return 1;
}
