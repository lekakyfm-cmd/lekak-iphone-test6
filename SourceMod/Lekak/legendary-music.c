#include <stdarg.h>
#include <stdio.h>
#include "pc/mods/modapi.h"
#include "pc/audio/replace.h"
#include "pc/mods/json.h"
#include "duelist-music-catalog.h"
#include "game/sound.h"
#include "game/sound_sequence_timing.h"
#include "game/duel_side_state.h"
extern signed char gDuel_bOpponentID;
extern int LekakBeforeMusicInit(const MemoriesModHost *,MemoriesMod *);
static const MemoriesModHost *mh;
static void *prev_bgm,*prev_play,*prev_pump;
static void (*prev_frame)(void);
static unsigned generation,handled;
static int chosen=-1;
static unsigned requested;
static int report;
static int loaded_track;
static unsigned tried_generation;
static void (*prev_applied)(int);
static void (*prev_shutdown)(void);
static void trace(const char *fmt,...){char b[256];va_list a;va_start(a,fmt);int n=vsnprintf(b,sizeof(b),fmt,a);va_end(a);FILE *f=mh->open_data(mh,"music-diagnostic.txt","a");if(f){if(n>0)fwrite(b,1,n<256?n:255,f);fclose(f);}}
/* The engine's JSON node layout is the same in API 9 and API 11. Build a
 * short-lived six-node audio object rather than retaining all 37 decoded
 * songs in the manifest. AudioReplace_Load consumes these nodes immediately. */
struct JsonValue {
 JsonType type;const char *name;char *text;long number;int boolean;
 JsonValue *first,*next;
};
typedef JsonValue MusicNode;
static int slot(int index){return index<3?0xfff0+index:LEKAK_MUSIC_SLOT;}
static void prepare_track(void){
 if(chosen<3||chosen>=LEKAK_MUSIC_COUNT||loaded_track==chosen+1||tried_generation==generation)return;
 tried_generation=generation;
 AudioReplace_Unload(LEKAK_MUSIC_OWNER);loaded_track=0;
 MusicNode volume={.type=JSON_NUMBER,.name="volume",.number=70};
 MusicNode loop={.type=JSON_BOOL,.name="loop",.boolean=1,.next=&volume};
 MusicNode file={.type=JSON_STRING,.name="file",.text=(char*)files[chosen],.next=&loop};
 MusicNode entry={.type=JSON_OBJECT,.name="0xFFEF",.first=&file};
 MusicNode group={.type=JSON_OBJECT,.name="music",.first=&entry};
 MusicNode root={.type=JSON_OBJECT,.first=&group};
 char why[256];
 if(AudioReplace_Load(LEKAK_MUSIC_OWNER,mh->id,mh->directory,(const JsonValue*)&root,why,sizeof(why))==1)
  loaded_track=chosen+1;
 else {handled=generation;if(report)trace("Cannot load %s: %s\n",files[chosen],why);}
}
static void bgm(u32 id){
 requested=id&0xfff;chosen=-1;
 if(requested==(gDuel_wBgmId&0xfff))
  for(int i=0;i<LEKAK_MUSIC_COUNT;i++)if(gDuel_bOpponentID>0&&gDuel_bOpponentID==mh->duelist_id(mh,names[i])){chosen=i;break;}
 if(report){trace("BGM request=%04x opponent=%d boss=%d\n",(unsigned)id,gDuel_bOpponentID,chosen);}
 ((void(*)(u32))prev_bgm)(id);
}
static void play(s32 start){
 ((void(*)(s32))prev_play)(start);
 if(start&255)generation++;
}
static void dispatch(void){
 if(generation==handled||!g_SDValue)return;
 if(chosen>=3&&loaded_track!=chosen+1)return;
 handled=generation;
 unsigned token=(u16)g_SDValue->field_157C;
 if(report){trace("Sequence=%04x requested=%04x boss=%d\n",token,requested,chosen);}
 if(chosen>=0&&(token&0xfff)==requested){
  int custom=slot(chosen),owner=AudioReplace_Owner(AUDIO_MUSIC,custom);
  if(report){trace("Custom slot=%04x owner=%d\n",custom,owner);}
  if(owner>=0)AudioReplace_MusicStart(custom);
 }
}
static void pump(void){((void(*)(void))prev_pump)();dispatch();}
/* Decode on the game frame, never in the sound interrupt's pump. */
static void frame(void){if(prev_frame)prev_frame();prepare_track();dispatch();}
static void music_applied(int on){
 if(!on){AudioReplace_Unload(LEKAK_MUSIC_OWNER);loaded_track=0;chosen=-1;tried_generation=0;}
 if(prev_applied)prev_applied(on);
}
static void music_shutdown(void){AudioReplace_Unload(LEKAK_MUSIC_OWNER);if(prev_shutdown)prev_shutdown();}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m){
 if(!LekakBeforeMusicInit(h,m))return 0;
 mh=h;prev_frame=m->frame;m->frame=frame;
 prev_applied=m->applied;m->applied=music_applied;
 prev_shutdown=m->shutdown;m->shutdown=music_shutdown;
 /* No storage writes from the sound interrupt or during normal play. */
 report=0;
 return h->hook(h,(void*)SD_BGMPlay,(void*)bgm,&prev_bgm)
 &&h->hook(h,(void*)SD_PlaySequence,(void*)play,&prev_play)
 &&h->hook(h,(void*)func_80045514,(void*)pump,&prev_pump);
}
