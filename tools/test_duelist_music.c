#include <assert.h>
#include <stdio.h>
#include <string.h>
/* The guest driver is a token fixture here; do not substitute host pointer
 * widths into the game's packed driver structs and their layout assertions. */
#include "../EngineSDK/src/types.h"
#define YUGIOH_GAME_SOUND_H
#define MEMORIES_DECOMP_SOUND_SEQUENCE_TIMING_H
#define MEMORIES_DECOMP_DUEL_SIDE_STATE_H
typedef struct {unsigned char prefix[0x157c];u16 field_157C;} MusicDriverFixture;
extern MusicDriverFixture *g_SDValue;
extern u16 gDuel_wBgmId;
void SD_BGMPlay(u32 id);void SD_PlaySequence(s32 start);void func_80045514(void);
#include "../SourceMod/Lekak/legendary-music.c"
signed char gDuel_bOpponentID;
u16 gDuel_wBgmId;
__typeof__(g_SDValue) g_SDValue;
static __typeof__(*g_SDValue) driver;
static int loads,unloads,starts,bgm_calls,active,fail_load;
static int resolve(const MemoriesModHost *h,const char *name){
 (void)h;for(int i=0;i<LEKAK_MUSIC_COUNT;i++)if(!strcmp(name,names[i]))return i+40;return 0;
}
static void stock_bgm(u32 id){(void)id;bgm_calls++;}
int AudioReplace_Load(int owner,const char *id,const char *directory,const JsonValue *root,char *error,size_t size){
 assert(owner==LEKAK_MUSIC_OWNER);assert(!strcmp(id,"Lekakmod"));assert(!strcmp(directory,"fixture"));
 /* These are the genuine engine JSON accessors, compiled from json.c. */
 assert(Json_TypeOf(root)==JSON_OBJECT);const JsonValue *group=Json_Member(root,"music");assert(Json_Count(group)==1);
 const JsonValue *entry=Json_At(group,0);assert(!strcmp(Json_Name(entry),"0xFFEF"));
 assert(!strcmp(Json_String(Json_Member(entry,"file"),NULL),files[chosen]));
 assert(Json_Bool(Json_Member(entry,"loop"),0));assert(Json_Number(Json_Member(entry,"volume"),0)==70);
 loads++;if(fail_load){snprintf(error,size,"fixture decode failure");return 0;}active=1;return 1;
}
void AudioReplace_Unload(int owner){assert(owner==LEKAK_MUSIC_OWNER);active=0;unloads++;}
int AudioReplace_Owner(AudioKind kind,int id){assert(kind==AUDIO_MUSIC);return id>=0xfff0?0:active?LEKAK_MUSIC_OWNER:-1;}
void AudioReplace_MusicStart(int id){assert(id==slot(chosen));starts++;}
int main(void){
 MemoriesModHost host={0};host.id="Lekakmod";host.directory="fixture";host.duelist_id=resolve;
 mh=&host;prev_bgm=(void*)stock_bgm;g_SDValue=&driver;gDuel_wBgmId=0x2d0;
 /* Opening the mod must not decode any of the new tracks. */
 assert(loads==0);for(int i=0;i<LEKAK_MUSIC_COUNT;i++){
  gDuel_bOpponentID=i+40;bgm(0x2d0);assert(chosen==i);generation++;driver.field_157C=0x2d0;
  int before=starts;dispatch();if(i>=3){assert(starts==before);frame();}assert(starts==before+1);
  for(int t=0;t<60;t++)frame();assert(starts==before+1);
 }
 assert(loads==37&&unloads==37);assert(bgm_calls==40);
 /* A repeated duel keeps its one decoded clip; unrelated music is stock. */
 int n=loads;bgm(0x2d0);generation++;frame();assert(loads==n);
 bgm(0x2d1);assert(chosen==-1);generation++;int before=starts;frame();assert(starts==before);
 gDuel_bOpponentID=1;bgm(0x2d0);assert(chosen==-1);
 /* A missing/invalid track falls back once; no per-frame retry storm. */
 gDuel_bOpponentID=40+3;bgm(0x2d0);generation++;fail_load=1;frame();n=loads;
 for(int t=0;t<60;t++)frame();assert(loads==n&&handled==generation);
 music_applied(0);assert(!active&&chosen==-1);music_shutdown();
 puts("40 duelist mappings passed; 37 deferred loads; one active added track; legendary slots unchanged; failure fallback and cleanup passed.");
 return 0;
}
