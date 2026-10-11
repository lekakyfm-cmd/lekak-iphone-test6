extern void LekakResults_Publish(int visible);
#include "pc/mods/modapi.h"
#include <stdint.h>
static const MemoriesModHost *qr_host;
static void (*qr_original_result)(void),(*qr_original_run)(void),(*qr_original_edit)(void);
static void (*qr_prepare_duel)(int,int,int,int);
static uint16_t *(*qr_record_slot)(void*,int);
static int (*qr_record_cap)(void);
static void (*qr_enter_deck)(void);
static uint16_t *qr_player_deck;
static uint8_t *qr_return_flags,*qr_result_side,*qr_editor_return;
static uint16_t *qr_scene_flags;
static volatile uint16_t *qr_pressed;
static uint8_t *qr_next_mode,*qr_active_mode,*qr_duel_phase,*qr_render_mode;
static int8_t *qr_opponent,*qr_player_side;
static int qr_shown,qr_pending,qr_pending_action,qr_editing;
static void qr_reset(void){qr_shown=0;qr_pending=-1;qr_pending_action=2;qr_editing=-1;LekakResults_Publish(0);}
static int qr_eligible(void){return *qr_next_mode==6 && *qr_opponent>=0 && *qr_player_side<0;}
static void qr_result(void){
    if(!(*qr_scene_flags&0x8000)){qr_reset();}
    if(qr_eligible() && (*qr_scene_flags&0x8000) && !(*qr_scene_flags&0x4000)){
        qr_shown=1;
        uint16_t actions=*qr_pressed&0x70;
        if(actions){
            /* Circle exits; Cross rematches; Triangle edits then rematches. */
            qr_pending_action=(actions&0x20)?2:((actions&0x10)?1:0);
            qr_pending=qr_pending_action<2?*qr_opponent:-1;
            qr_shown=0;
            /* Let the normal qr_result handler perform its usual exit. */
            *qr_pressed=(*qr_pressed&~0x70u)|0x40;
        }
    }else qr_shown=0;
    LekakResults_Publish(qr_shown);
    qr_original_result();
}
static void qr_restart_duel(int id){
    qr_prepare_duel(-1,id,0x6000,0x6000);
    *qr_return_flags|=0x80;
    *qr_active_mode=3|0x40;*qr_duel_phase=1;*qr_render_mode=10;
}
static void qr_edit(void){
    qr_original_edit();
    if(qr_editing>=0 && *qr_active_mode==6){
        int id=qr_editing,i,valid=1;qr_editing=-1;
        for(i=0;i<40;i++)if(!qr_player_deck[i])valid=0;
        if(valid)qr_restart_duel(id);
    }
}
static void qr_run(void){
    int leaving=(*qr_duel_phase&15)==2;
    int old_opponent=*qr_opponent;
    if(!(*qr_active_mode&0x40))qr_reset();
    qr_original_run();
    if(leaving){
        qr_shown=0;LekakResults_Publish(0);
        if(*qr_active_mode==6 && *qr_next_mode==6 && qr_pending==old_opponent && old_opponent>=0){
            /* All original rewards and normal teardown have already qr_run. */
            /* FreeDuel_Init normally records this qr_result. We skip that visit. */
            if(*qr_return_flags&0x80){
                uint16_t *slot=qr_record_slot(qr_player_deck,old_opponent)+(*qr_result_side==1);
                int cap=qr_record_cap();
                if((int16_t)*slot<cap)*slot=(uint16_t)(*slot+1);
                else *slot=(uint16_t)cap;
                *qr_return_flags&=~0x80u;
            }
            if(qr_pending_action==1){
                qr_editing=old_opponent;qr_enter_deck();*qr_editor_return=6;
            }else qr_restart_duel(old_opponent);
        }
        qr_pending=-1;
    }
}
static unsigned qr_signature(void){return (unsigned)qr_shown;}
/* A 5x7 bitmap font avoids the host overlay's minimum 20-pixel font,
 * which is enlarged again with the 320x240 game image on iPhone. */
static const unsigned char qr_letters[26][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},
 {14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
 {31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
 {14,4,4,4,4,4,14},{7,2,2,2,18,18,12},
 {17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},
 {14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
 {14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
 {17,17,17,17,17,17,14},{17,17,17,17,17,10,4},
 {17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static int qr_small_width(const char *s,int pixel){
 int n=0;while(s[n])n++;return (n?6*n-1:0)*pixel;
}
static void qr_small_text(int x,int y,const char *s,uint32_t color,int pixel){
 static const unsigned char slash[7]={1,2,2,4,8,8,16};
 static const unsigned char triangle[7]={4,4,10,10,17,17,31};
 for(;*s;s++,x+=6*pixel){
  const unsigned char *glyph=0;
  if(*s>='A' && *s<='Z')glyph=qr_letters[*s-'A'];
  else if(*s=='/')glyph=slash;
  else if(*s=='^')glyph=triangle;
  if(glyph)for(int row=0;row<7;row++)for(int col=0;col<5;col++)
   if(glyph[row]&(16>>col))qr_host->fill(qr_host,x+col*pixel,y+row*pixel,pixel,pixel,color,255);
 }
}
static void qr_overlay(void){
 static const char *const labels[]={"O QUITTER / EXIT","X REJOUER / REMATCH","^ DECK / REMATCH"};
 static const uint32_t colors[]={0xFF9999,0x99CCFF,0xFFAAFF};
 int w,h,unused,pixel,x,y,width=0,height;
 if(!qr_shown)return;
 qr_host->overlay_size(qr_host,&w,&h,&unused);
 if(w<160 || h<100)return;
 pixel=w/320;if(pixel<1)pixel=1;
 if(h/240>0 && pixel>h/240)pixel=h/240;
 for(int i=0;i<3;i++){int n=qr_small_width(labels[i],pixel);if(n>width)width=n;}
 width+=12*pixel;height=35*pixel;
 x=(w-width)/2;y=(h-height)/2;
 qr_host->fill(qr_host,x-1,y-1,width+2,height+2,0xCCBBDD,255);
 qr_host->fill(qr_host,x,y,width,height,0x17101D,245);
 for(int i=0;i<3;i++)qr_small_text(x+6*pixel,y+(4+10*i)*pixel,labels[i],colors[i],pixel);
}
int QuickRematchInit(const MemoriesModHost *api,MemoriesMod *mod){
    void *fn_result,*fn_run,*fn_edit;int a,b,c;
    if(api->api<4)return 0;
    qr_host=api;
#define RESOLVE(pointer,name) pointer=qr_host->symbol(qr_host,name);if(!pointer)return 0
    RESOLVE(qr_record_slot,"Duelists_RecordSlot");
    RESOLVE(qr_record_cap,"Tables_FreeDuelRecordCap");
    RESOLVE(qr_player_deck,"gDuel_awPlayerDeck");
    RESOLVE(qr_return_flags,"gFreeDuel_bReturnFlags");
    RESOLVE(qr_result_side,"D_8009B362");
    RESOLVE(qr_editor_return,"D_8009B269");
    RESOLVE(qr_enter_deck,"BuildDeck_EnterNarrowConfirmMode");
    RESOLVE(fn_edit,"Main_RunBuildDeckMenu");
    RESOLVE(qr_scene_flags,"gDuel_wSceneStateFlags");
    RESOLVE(qr_pressed,"gInput_wPad1Pressed");
    RESOLVE(qr_next_mode,"D_8009B368");
    RESOLVE(qr_active_mode,"D_8009B26C");
    RESOLVE(qr_duel_phase,"D_8009B26E");
    RESOLVE(qr_render_mode,"D_8009B0A3");
    RESOLVE(qr_opponent,"gDuel_bOpponentID");
    RESOLVE(qr_player_side,"D_8009B360");
    RESOLVE(qr_prepare_duel,"func_80024DC8");
    RESOLVE(fn_result,"DuelScene_UpdateResultRewards");
    RESOLVE(fn_run,"Main_RunDuel");
#undef RESOLVE
    a=qr_host->hook(qr_host,fn_result,qr_result,(void**)&qr_original_result);if(!a)return 0;
    b=qr_host->hook(qr_host,fn_run,qr_run,(void**)&qr_original_run);
    if(!b){qr_host->unhook(qr_host,a);return 0;}
    c=qr_host->hook(qr_host,fn_edit,qr_edit,(void**)&qr_original_edit);
    if(!c){qr_host->unhook(qr_host,b);qr_host->unhook(qr_host,a);return 0;}
    mod->api=4;mod->name="Lekak - Quick rematch test";
    mod->reset=qr_reset;mod->overlay=qr_overlay;mod->overlay_signature=qr_signature;
    qr_reset();return 1;
}

#define QR_BASE LekakMusicBaseInit
#define QR_ENTRY MemoriesModInit

static MemoriesMod qr_extension;
static void (*qr_base_reset)(void),(*qr_base_overlay)(void);
static unsigned (*qr_base_signature)(void);
static unsigned qr_frame_generation;
static void qr_combined_reset(void){if(qr_base_reset)qr_base_reset();qr_extension.reset();}
static void qr_combined_overlay(void){if(qr_base_overlay)qr_base_overlay();qr_extension.overlay();}
static unsigned qr_combined_signature(void){
    unsigned a=qr_base_signature?qr_base_signature():(qr_base_overlay?++qr_frame_generation:0);
    return a*2654435761u+qr_extension.overlay_signature();
}
int QR_ENTRY(const MemoriesModHost *api,MemoriesMod *mod){
    if(!QR_BASE(api,mod))return 0;
    if(!QuickRematchInit(api,&qr_extension))return 0;
    qr_base_reset=mod->reset;qr_base_overlay=mod->overlay;qr_base_signature=mod->overlay_signature;
    mod->reset=qr_combined_reset;mod->overlay=qr_combined_overlay;
    mod->overlay_signature=qr_combined_signature;
    return 1;
}
