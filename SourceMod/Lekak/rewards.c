#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "pc/free_duel/duelists.h"
#include "game/input.h"
#include "game/display_object.h"
#include "overlays/main_menu/frontend.h"
#include "game/duel_result_display.h"
#include "pc/platform/title_screen.h"
#include <string.h>
#include <stdio.h>
#include "rewards-logic.h"
#include "rewards-data.h"
/* SAVE_DATA_RESERVED_TAIL_OFFSET=0x628, size=0x58. The game's payload
 * builder clears this unsealed tail; preserve our verified ledger after
 * it builds both identical copies. No card quantities or stock CRC move. */
#define TAIL_OFFSET 0x628
#define STATE_OFFSET 0x200
#define DUP_OFFSET 0x880
static const MemoriesModHost *host;
static uint8_t *save_base;
static int8_t *opponent,*winner;
static uint32_t *starchips;
static void (*old_build)(void *);
static void (*old_apply)(void *);
static int (*old_title)(void);
static unsigned serial=1;
static int opened,category,row,english,corrupt;
#define AVAILABLE_CATEGORY (ROSTER_COUNT+1)
#define CATEGORY_COUNT (ROSTER_COUNT+2)
static int available_keys[ROSTER_COUNT*5+GROUP_COUNT],available_count;
static char notice[112];
static const int goals[4]={1,10,25,50};
static const unsigned prizes[5]={20,100,300,600,100};
static RewardSave *ledger(void){return (RewardSave *)(save_base+TAIL_OFFSET);}
static uint32_t save_code(void){uint32_t n;memcpy(&n,save_base+0x334,4);return n;}
static RewardSave *ready(void){
 RewardSave *s=ledger();uint32_t code=save_code();
 corrupt=0;
 if(s->magic==REWARD_MAGIC && s->duelist_code==code){
  if(!reward_valid(s,code)){corrupt=1;return 0;}
 }else{
  memset(s,0,sizeof(*s));s->magic=REWARD_MAGIC;s->version=1;s->duelist_code=code;reward_seal(s);
 }
 return s;
}
static int duelist(int index){return Duelists_Named(roster[index].ref);}
static int wins(int index){int id=duelist(index);return id>0?Duelists_RecordSlot(save_base,id)[0]:0;}
static int group_progress(int g){
 const GroupSpec *r=&groups[g];int n=0;
 for(int i=0;i<r->count;i++)n+=wins(r->members[i])>=r->target;
 return n;
}
static int complete(int cat,int line,RewardSave *s){
 if(cat<ROSTER_COUNT)return line<4?wins(cat)>=goals[line]:reward_tec(s,cat);
 return line>=0&&line<GROUP_COUNT&&group_progress(line)==groups[line].count;
}
static int key_for(int cat,int line){return cat<ROSTER_COUNT?cat*5+line:ROSTER_COUNT*5+line;}
static void available(RewardSave *s){
 available_count=0;if(!s)return;
 for(int cat=0;cat<=ROSTER_COUNT;cat++){
  int count=cat<ROSTER_COUNT?5:GROUP_COUNT;
  for(int i=0;i<count;i++){
   int key=key_for(cat,i);
   if(!reward_claimed(s,key)&&complete(cat,i,s))available_keys[available_count++]=key;
  }
 }
 if(category==AVAILABLE_CATEGORY&&row>=available_count)row=available_count?available_count-1:0;
}
static int rows(void){return category==AVAILABLE_CATEGORY?available_count:category<ROSTER_COUNT?5:GROUP_COUNT;}
static int selection(int *cat,int *line){
 *cat=category;*line=row;
 if(category==AVAILABLE_CATEGORY){
  available(ready());if(!available_count)return 0;
  int key=available_keys[row];
  *cat=key<ROSTER_COUNT*5?key/5:ROSTER_COUNT;
  *line=key<ROSTER_COUNT*5?key%5:key-ROSTER_COUNT*5;
 }
 return 1;
}
static void build(void *payload){
 RewardSave copy;RewardSave *s=ready();int keep=s!=0;
 if(keep)memcpy(&copy,s,sizeof(copy));
 old_build(payload);
 if(keep){
  memcpy((uint8_t *)payload+STATE_OFFSET+TAIL_OFFSET,&copy,sizeof(copy));
  memcpy((uint8_t *)payload+DUP_OFFSET+TAIL_OFFSET,&copy,sizeof(copy));
  memcpy(ledger(),&copy,sizeof(copy));
 }
}
static void apply(void *state){
 RewardSave copy;memcpy(&copy,(uint8_t *)state+TAIL_OFFSET,sizeof(copy));
 old_apply(state);memcpy(ledger(),&copy,sizeof(copy));
 opened=0;notice[0]=0;serial++;
}
static void awarded(MemoriesModEvent *event){
 if(event->phase!=MEMORIES_AFTER||*winner!=0||!D_8009B1E8)return;
 if(!D_8009B1E8->is_tec_rank||D_8009B1E8->rank_tier<3)return;
 RewardSave *s=ready();if(!s)return;
 for(int i=0;i<ROSTER_COUNT;i++)if(duelist(i)==*opponent){reward_mark_tec(s,i);serial++;break;}
}
static int objectives_open(void){
 if(gMain_bMenuID<5||gMain_bMenuID>10||D_80184599||D_80184598||D_8018459A
  ||D_8018459B||D_8018459C||D_8018459D)return 0;
 int count=0;
 for(int i=5;i<11;i++){
  DisplayObject *o=(DisplayObject *)gMain_apMenuEntries[i];
  if(o){count++;if(o->flags&DISPLAY_OBJECT_FLAG_RENDERABLE)return 0;}
 }
 return count>=3;
}
static void message(const char *fr,const char *en){snprintf(notice,sizeof(notice),"%s",english?en:fr);serial++;}
static int reward_card(const GroupSpec *g){return g->card?Cards_Named(g->card):0;}
static void claim(void){
 RewardSave *s=ready();int cat,line;
 if(!selection(&cat,&line)){message("Aucune recompense disponible.","No rewards available.");return;}
 int key=key_for(cat,line);unsigned amount;
 if(!s){message("Suivi endommage : recharge une sauvegarde valide.","Ledger damaged: load a valid save.");return;}
 if(reward_claimed(s,key)){message("Deja recuperee sur cette sauvegarde.","Already claimed on this save.");return;}
 if(!complete(cat,line,s)){message("Objectif pas encore atteint.","Goal not yet completed.");return;}
 amount=cat<ROSTER_COUNT?prizes[line]:groups[line].stars;
 if(amount){
  if(!reward_stars_fit(*starchips,amount,Tables_StarchipCap())){
   message("Limite d'etoiles : depense avant de recuperer.","Star-chip limit: spend some before claiming.");return;
  }
  *starchips+=amount;
 }else{
  int id=reward_card(&groups[line]);
  if(id<=0||!Cards_Valid(id)){message("Carte indisponible : recompense conservee.","Card unavailable: reward remains unclaimed.");return;}
  uint8_t *quantity=Cards_ChestSlot(save_base,id);
  if(*quantity>=Tables_ChestRoom()){message("Coffre plein pour cette carte.","Trunk is full for this card.");return;}
  /* Quest rewards are not duel drops: do not invoke drop hooks or alter
     the last-duel display and recent-drop history. */
  (*quantity)++;Cards_MarkSeen(id);
 }
 reward_mark(s,key);available(s);
 message("Recuperee ! Pense a sauvegarder ta partie.","Claimed! Remember to save your game.");
}
static int title(void){
 uint16_t p=gInput_wPad1Pressed,n=p|gInput_wPad1Repeat;
 if(!opened){
  if(objectives_open()&&(p&PAD_BUTTON_SQUARE)){
   opened=1;category=AVAILABLE_CATEGORY;row=0;notice[0]=0;available(ready());serial++;
   gInput_wPad1Pressed=gInput_wPad1Held=gInput_wPad1Repeat=0;return -1;
  }
  return old_title();
 }
 if(p&PAD_BUTTON_CANCEL){opened=0;notice[0]=0;serial++;}
 else if(p&PAD_BUTTON_TRIANGLE){english=!english;host->set_setting(host,"language",english);notice[0]=0;serial++;}
 else if(p&PAD_BUTTON_SQUARE){category=AVAILABLE_CATEGORY;row=0;available(ready());notice[0]=0;serial++;}
 else if(p&(PAD_BUTTON_R1|PAD_DIRECTION_RIGHT)){category=(category+1)%CATEGORY_COUNT;row=0;notice[0]=0;serial++;}
 else if(p&(PAD_BUTTON_L1|PAD_DIRECTION_LEFT)){category=(category+CATEGORY_COUNT-1)%CATEGORY_COUNT;row=0;notice[0]=0;serial++;}
 else if(n&(PAD_DIRECTION_DOWN|PAD_BUTTON_R2)){int count=rows();if(count)row=(row+((n&PAD_BUTTON_R2)?5:1))%count;notice[0]=0;serial++;}
 else if(n&(PAD_DIRECTION_UP|PAD_BUTTON_L2)){int count=rows();if(count)row=(row+count-(((n&PAD_BUTTON_L2)?5:1)%count))%count;notice[0]=0;serial++;}
 else if(p&PAD_BUTTON_CROSS)claim();
 gInput_wPad1Pressed=gInput_wPad1Held=gInput_wPad1Repeat=0;
 return -1;
}
static int text_right;
static void text(int x,int y,const char *value,unsigned color,int scale){
 char clipped[160];snprintf(clipped,sizeof(clipped),"%s",value);
 int n=strlen(clipped),cut=0;
 while(n>0 && host->text_width(host,clipped,scale)>text_right-x){clipped[--n]=0;cut=1;}
 if(cut&&n>=3){clipped[n-1]=clipped[n-2]=clipped[n-3]='.';}
 host->draw_text(host,x,y,clipped,color,scale);
}
static void overlay(void){
 if(!opened)return;
 int w,h,s;host->overlay_size(host,&w,&h,&s);
 s=w/640;if(s>h/400)s=h/400;if(s<1)s=1;
 int width=580*s,height=350*s;
 if(width>w-16)width=w-16;
 if(height>h-16)height=h-16;
 int x=(w-width)/2,y=(h-height)/2;
 text_right=x+width-12*s;
 /* Bitmap fallback glyphs are 14*s tall. Reserve 18*s per line,
  * 40*s per two-line reward, and paginate according to real canvas size. */
 int header=72*s,bottom=68*s,step=40*s;
 int visible=(height-header-bottom)/step;
 if(visible<1)visible=1;if(visible>5)visible=5;
 RewardSave *state=ready();char line[160];
 available(state);
 host->fill(host,0,0,w,h,0x100B17,255);
 host->fill(host,x-2,y-2,width+4,height+4,0xB9A8CD,255);
 host->fill(host,x,y,width,height,0x1B1325,255);
 text(x+16*s,y+20*s,english?"LEKAK - REWARDS":"LEKAK - RECOMPENSES",0xEEE4FF,s);
 if(category==AVAILABLE_CATEGORY)snprintf(line,sizeof(line),english?"AVAILABLE (%d)":"DISPONIBLES (%d)",available_count);
 else snprintf(line,sizeof(line),"%s   %d/%d",category<ROSTER_COUNT?roster[category].name:(english?"SPECIAL GOALS":"OBJECTIFS SPECIAUX"),category+1,CATEGORY_COUNT);
 text(x+16*s,y+40*s,line,0x9ACDFF,s);
 snprintf(line,sizeof(line),english?"Star chips: %u":"Etoiles : %u",*starchips);
 text(x+16*s,y+60*s,line,0xFFF1A0,s);
 int start=(row/visible)*visible;
 if(category==AVAILABLE_CATEGORY&&!available_count)text(x+20*s,y+header+30*s,english?"No rewards available yet.":"Aucune recompense disponible pour le moment.",0xC9BAD8,s);
 for(int k=0;k<visible;k++){
  if(category==AVAILABLE_CATEGORY){
   int index=start+k;if(index>=available_count)break;
   int key=available_keys[index],cat=key<ROSTER_COUNT*5?key/5:ROSTER_COUNT;
   int goal=key<ROSTER_COUNT*5?key%5:key-ROSTER_COUNT*5;
   int yy=y+header+8*s+k*step;
   if(index==row)host->fill(host,x+10*s,yy-12*s,width-20*s,step-3*s,0x413051,255);
   if(cat<ROSTER_COUNT){
    if(goal<4)snprintf(line,sizeof(line),"%s %s : %d %s",index==row?">":" ",roster[cat].name,goals[goal],english?"wins":"victoires");
    else snprintf(line,sizeof(line),"%s %s : S/A TEC",index==row?">":" ",roster[cat].name);
    text(x+20*s,yy,line,0x91FFAE,s);
    snprintf(line,sizeof(line),english?"  +%u star chips":"  +%u etoiles",prizes[goal]);
   }else{
    const GroupSpec *g=&groups[goal];
    snprintf(line,sizeof(line),"%s %s",index==row?">":" ",english?g->en:g->name);text(x+20*s,yy,line,0x91FFAE,s);
    if(g->stars)snprintf(line,sizeof(line),english?"  +%u star chips":"  +%u etoiles",g->stars);
    else snprintf(line,sizeof(line),"  %s x1",g->card_name);
   }
   text(x+20*s,yy+18*s,line,0xC9BAD8,s);continue;
  }
  int index=start+k;if(category<ROSTER_COUNT&&index>=5)break;if(category>=ROSTER_COUNT&&index>=GROUP_COUNT)break;
  int yy=y+header+8*s+k*step,key=key_for(category,index),taken=state&&reward_claimed(state,key);
  int done=state&&complete(category,index,state);
  unsigned color=taken?0xA9A0B4:done?0x91FFAE:0xE1D9E9;
  if(index==row)host->fill(host,x+10*s,yy-12*s,width-20*s,step-3*s,0x413051,255);
  const char *status=english?(taken?"CLAIMED":done?"READY":"LOCKED"):(taken?"RECUPEREE":done?"PRETE":"EN COURS");
  if(category<ROSTER_COUNT){
   if(index<4)snprintf(line,sizeof(line),"%s %d %s  (%d/%d)  [%s]",index==row?">":" ",goals[index],english?"wins":"victoires",wins(category)>goals[index]?goals[index]:wins(category),goals[index],status);
   else snprintf(line,sizeof(line),"%s %s  (%d/1)  [%s]",index==row?">":" ",english?"First S/A TEC win":"Premier S/A TEC",state&&reward_tec(state,category)?1:0,status);
   text(x+20*s,yy,line,color,s);
   snprintf(line,sizeof(line),english?"  +%u star chips":"  +%u etoiles",prizes[index]);
  }else{
   const GroupSpec *g=&groups[index];
   snprintf(line,sizeof(line),"%s %s  (%d/%d) [%s]",index==row?">":" ",english?g->en:g->name,group_progress(index),g->count,status);text(x+20*s,yy,line,color,s);
   if(g->stars)snprintf(line,sizeof(line),english?"  %d wins each -> %u star chips":"  %d victoire(s) chacun -> %u etoiles",g->target,g->stars);
   else snprintf(line,sizeof(line),"  %d %s -> %s x1",g->target,english?"wins each":"victoires chacun",g->card_name);
  }
  text(x+20*s,yy+18*s,line,0xC9BAD8,s);
 }
 if(corrupt)text(x+16*s,y+height-58*s,english?"Ledger damaged.":"Suivi endommage.",0xFF9999,s);
 else if(notice[0])text(x+16*s,y+height-58*s,notice,0xFFF1A0,s);
 else text(x+16*s,y+height-58*s,english?"Save after claiming.":"Sauvegarde apres.",0xC9BAD8,s);
 text(x+16*s,y+height-38*s,english?"X: claim  O: back":"X: prendre O: retour",0xEEE4FF,s);
 text(x+16*s,y+height-18*s,english?"L1/R1: tabs  Tri: FR":"L1/R1: pages Tri: EN",0x9ACDFF,s);
}
static unsigned signature(void){return serial;}
static void reset(void){opened=0;notice[0]=0;serial++;}
static void applied(int on){(void)on;reset();}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m){
 if(h->api<6||!h->subscribe||!h->hook||!h->unhook||!h->overlay_size)return 0;host=h;
 save_base=h->symbol(h,"gDuel_awPlayerDeck");opponent=h->symbol(h,"gDuel_bOpponentID");
 winner=h->symbol(h,"gDuel_bWinnerSide");starchips=h->symbol(h,"gLibrary_dwStarchips");
 if(!save_base||!opponent||!winner||!starchips)return 0;
 void *fn=h->symbol(h,"SaveData_BuildPayload");
 int build_token=fn?h->hook(h,fn,build,(void **)&old_build):0;
 if(!build_token)return 0;
 fn=h->symbol(h,"SaveData_ApplyRuntimeState");
 int apply_token=fn?h->hook(h,fn,apply,(void **)&old_apply):0;
 if(!apply_token){h->unhook(h,build_token);return 0;}
 int title_token=h->hook(h,TitleScreen_Update,title,(void **)&old_title);
 if(!title_token){h->unhook(h,apply_token);h->unhook(h,build_token);return 0;}
 if(!h->subscribe(h,MEMORIES_EVENT_STARCHIP,0,awarded)){
  h->unhook(h,title_token);h->unhook(h,apply_token);h->unhook(h,build_token);return 0;
 }
 english=h->setting(h,"language",0)!=0;
 m->api=6;m->name="Lekak - Rewards iPhone test v1.1";m->overlay=overlay;
 m->overlay_signature=signature;m->reset=reset;m->applied=applied;
 if(h->log)h->log(h,"Rewards v1: 409 permanent goals, save-tail ledger, Square inside Objectives");
 return 1;
}
