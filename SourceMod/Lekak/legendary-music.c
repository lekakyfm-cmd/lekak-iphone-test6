#include "pc/mods/modapi.h"
#include "pc/audio/replace.h"
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
static void trace(const char *fmt,...){char b[256];va_list a;va_start(a,fmt);int n=vsnprintf(b,sizeof(b),fmt,a);va_end(a);FILE *f=mh->open_data(mh,"music-diagnostic.txt","a");if(f){if(n>0)fwrite(b,1,n<256?n:255,f);fclose(f);}}
static const char *names[]={"Lekakmod:lesub","Lekakmod:yem","Lekakmod:lekak"};
static void bgm(u32 id){
 requested=id&0xfff;chosen=-1;
 if(requested==(gDuel_wBgmId&0xfff))
  for(int i=0;i<3;i++)if(gDuel_bOpponentID>0&&gDuel_bOpponentID==mh->duelist_id(mh,names[i]))chosen=i;
 if(report){trace("BGM request=%04x opponent=%d boss=%d\n",(unsigned)id,gDuel_bOpponentID,chosen);}
 ((void(*)(u32))prev_bgm)(id);
}
static void play(s32 start){
 ((void(*)(s32))prev_play)(start);
 if(start&255)generation++;
}
static void dispatch(void){
 if(generation==handled||!g_SDValue)return;
 handled=generation;
 unsigned token=(u16)g_SDValue->field_157C;
 if(report){trace("Sequence=%04x requested=%04x boss=%d\n",token,requested,chosen);}
 if(chosen>=0&&(token&0xfff)==requested){
  int slot=0xfff0+chosen,owner=AudioReplace_Owner(AUDIO_MUSIC,slot);
  if(report){trace("Custom slot=%04x owner=%d\n",slot,owner);}
  if(owner>=0)AudioReplace_MusicStart(slot);
 }
}
static void pump(void){((void(*)(void))prev_pump)();dispatch();}
static void frame(void){if(prev_frame)prev_frame();dispatch();}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m){
 if(!LekakBeforeMusicInit(h,m))return 0;
 mh=h;prev_frame=m->frame;m->frame=frame;
 FILE *f=h->open_data(h,"music-diagnostic.txt","w");report=f!=0;if(f)fclose(f);
 if(report){for(int i=0;i<3;i++)trace("Loaded %s slot=%04x owner=%d\n",names[i],0xfff0+i,AudioReplace_Owner(AUDIO_MUSIC,0xfff0+i));}
 return h->hook(h,(void*)SD_BGMPlay,(void*)bgm,&prev_bgm)
 &&h->hook(h,(void*)SD_PlaySequence,(void*)play,&prev_play)
 &&h->hook(h,(void*)func_80045514,(void*)pump,&prev_pump);
}
