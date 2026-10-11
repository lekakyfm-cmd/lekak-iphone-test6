#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "pc/mods/modapi.h"
static int base_resets;
static void base_reset(void){base_resets++;}
int LekakMusicBaseInit(const MemoriesModHost*a,MemoriesMod*m){(void)a;m->reset=base_reset;return 1;}
static int panel_visible;
void LekakResults_Publish(int v){panel_visible=v;}
#include "../SourceMod/Lekak/quick-rematch.h"
static uint16_t flags;
static struct {uint16_t keys,backup;} keys;
static uint8_t next=6,active=67,phase=0x81,render=0;
static int8_t opp=7,side=-1;
static uint8_t ret=0x80,res=0,back=6;
static uint16_t deck[40],records[80][2];
static uint16_t *record(void*state,int id){assert(state==deck && id>=0 && id<80);return records[id];}
static int cap(void){return 999;}
static void open_editor(void){active=7;}
static void retail_edit(void){deck[0]=99;active=back;}
static void record_return(void){if(ret&0x80){records[opp][res==1]++;ret&=~0x80;}}
static int rewards,teardown,restarted,win=1;
static void retail_result(void){
 flags|=0x8000;
 if(keys.keys&0x40)flags|=0x4000;
}
static void retail_run(void){
 if((phase&15)==2){rewards+=win;teardown++;active=next;}
}
static void prepare(int a,int b,int c,int d){
 assert(teardown>0 && a==-1 && b==opp && c==0x6000 && d==0x6000);restarted++;
}
static void *symbol(const MemoriesModHost *h,const char *s){
 (void)h;
#define SYM(n,p) if(!strcmp(s,n))return p
 SYM("Duelists_RecordSlot",record);SYM("Tables_FreeDuelRecordCap",cap);
 SYM("gDuel_awPlayerDeck",deck);SYM("gFreeDuel_bReturnFlags",&ret);SYM("D_8009B362",&res);
 SYM("D_8009B269",&back);SYM("BuildDeck_EnterNarrowConfirmMode",open_editor);SYM("Main_RunBuildDeckMenu",retail_edit);
 SYM("gDuel_wSceneStateFlags",&flags);SYM("gInput_wPad1Pressed",&keys.keys);
 SYM("D_8009B368",&next);SYM("D_8009B26C",&active);SYM("D_8009B26E",&phase);
 SYM("D_8009B0A3",&render);SYM("gDuel_bOpponentID",&opp);SYM("D_8009B360",&side);
 SYM("func_80024DC8",prepare);SYM("DuelScene_UpdateResultRewards",retail_result);
 SYM("Main_RunDuel",retail_run);
#undef SYM
 return 0;
}
static int hook(const MemoriesModHost*h,void*f,void*r,void**o){(void)h;(void)r;*o=f;return 1;}
static void unhook(const MemoriesModHost*h,int t){(void)h;(void)t;}
static void begin(void){qr_reset();flags=0x8000;keys.keys=0;keys.backup=0xABCD;next=6;active=67;phase=0x81;opp=7;side=-1;ret=0x80;res=0;for(int i=0;i<40;i++)deck[i]=1;}
static int screen_w=1916,screen_h=970,lines,panels,last_scale;
static void size_mock(const MemoriesModHost*h,int*w,int*t,int*s){(void)h;*w=screen_w;*t=screen_h;*s=1;}
static int width_mock(const MemoriesModHost*h,const char*s,int scale){(void)h;return (int)strlen(s)*8*scale;}
static void fill_mock(const MemoriesModHost*h,int x,int y,int w,int t,uint32_t rgb,unsigned a){
 (void)h;(void)rgb;(void)a;assert(x>=0 && y>=0 && x+w<=screen_w && y+t<=screen_h);panels++;
}
static void text_mock(const MemoriesModHost*h,int x,int y,const char*s,uint32_t rgb,int scale){
 (void)h;(void)rgb;assert(x>=0 && y>=0 && x+(int)strlen(s)*8*scale<=screen_w);last_scale=scale;lines++;
}
int main(void){
 MemoriesModHost api={0};MemoriesMod mod={0};api.api=9;api.symbol=symbol;api.hook=hook;api.unhook=unhook;api.overlay_size=size_mock;api.text_width=width_mock;api.fill=fill_mock;api.draw_text=text_mock;
 assert(MemoriesModInit(&api,&mod));mod.reset();assert(base_resets==1);
 begin();qr_result();assert(qr_shown && panel_visible);qr_overlay();assert(lines==3 && panels==2 && last_scale==1);
 screen_w=640;screen_h=480;qr_overlay();assert(lines==6 && last_scale==1);keys.keys=0x20;qr_result();
 assert(qr_pending==-1 && (flags&0x4000));phase=2;qr_run();assert(!restarted && rewards==1 && active==6);
 record_return();assert(records[7][0]==1);
 begin();keys.keys=0x1000;qr_result();assert(keys.keys==0x1000 && keys.backup==0xABCD);
 keys.keys=0x40;qr_result();assert(qr_pending==7 && !qr_shown);phase=2;qr_run();
 assert(restarted==1 && rewards==2 && records[7][0]==2 && active==67 && phase==1 && render==10);qr_run();assert(rewards==2);
 /* The final ordinary return of a chain records its own match, not an earlier one. */
 flags=0x8000;keys.keys=0;qr_result();keys.keys=0x20;qr_result();phase=2;qr_run();record_return();
 assert(records[7][0]==3);
 begin();opp=79;keys.keys=0x40;qr_result();assert(qr_pending==79);phase=2;qr_run();assert(restarted==2 && records[79][0]==1);
 begin();win=0;res=1;keys.keys=0x40;qr_result();phase=2;qr_run();assert(restarted==3 && records[7][1]==1);
 begin();next=2;keys.keys=0x40;qr_result();assert(!qr_shown && qr_pending==-1);phase=2;qr_run();assert(active==2 && restarted==3);
 begin();opp=-1;qr_result();assert(!qr_shown);
 begin();side=0;qr_result();assert(!qr_shown);
 begin();win=1;keys.keys=0x10;qr_result();assert(qr_pending_action==1 && keys.keys==0x40 && keys.backup==0xABCD);phase=2;qr_run();
 assert(active==7 && qr_editing==7 && !(ret&0x80) && records[7][0]==4);
 qr_edit();assert(deck[0]==99 && active==67 && restarted==4 && (ret&0x80));
 /* Repeated calls during the new match do not credit it before completion. */
 qr_run();assert(records[7][0]==4);
 begin();records[7][0]=999;keys.keys=0x40;qr_result();phase=2;qr_run();assert(records[7][0]==999);
 begin();keys.keys=0x70;qr_result();assert(qr_pending==-1 && qr_pending_action==2 && keys.keys==0x40 && keys.backup==0xABCD);
 puts("PASS: chain wins + final return, losses, record cap, deck editor + rematch, existing reward path, input width");
}
