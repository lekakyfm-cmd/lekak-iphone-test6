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
static void qr_overlay(void){

    static const uint32_t colors[]={0xFF9999,0x99CCFF,0xFFAAFF};
    int w,h,scale,x,y,width=0,height,i;
    if(!qr_shown)return;
    qr_host->overlay_size(qr_host,&w,&h,&scale);
    /* Size against the displayed window, independent of the qr_host's tiny default. */
    scale=w>=1000 && h>=600?2:1;
    for(i=0;i<3;i++){
        int n=qr_host->text_width(qr_host,(i==0?"O  QUITTER / EXIT":i==1?"X  REJOUER / REMATCH":"/\\ DECK + REJOUER / REMATCH"),scale);
        if(n>width)width=n;
    }
    width+=24*scale;height=62*scale;
    x=(w-width)/2;y=h/2-height/2;
    qr_host->fill(qr_host,x-2,y-2,width+4,height+4,0xCCBBDD,255);
    qr_host->fill(qr_host,x,y,width,height,0x17101D,245);
    for(i=0;i<3;i++)qr_host->draw_text(qr_host,x+12*scale,y+(13+18*i)*scale,(i==0?"O  QUITTER / EXIT":i==1?"X  REJOUER / REMATCH":"/\\ DECK + REJOUER / REMATCH"),colors[i],scale);
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
