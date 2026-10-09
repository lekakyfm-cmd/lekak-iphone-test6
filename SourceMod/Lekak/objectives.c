#include "pc/mods/modapi.h"
#include "overlays/main_menu/frontend.h"
#include "game/input.h"
#include "game/display_object.h"
#include "game/display_object_config.h"
#include "game/text_box_lifecycle.h"
#include "game/text_box_runtime.h"
#include "game/save_data.h"
#include "pc/free_duel/duelists.h"
#include "pc/text/text.h"
#ifdef MEMORIES_IOS
#include "pc/platform/title_screen.h"
#endif

typedef struct { const char *identity,*name,*requirements; } Objective;
#include "objectives-data.h"
extern int LekakButton_Init(const MemoriesModHost *);
extern void LekakButton_Restore(void),LekakButton_Reset(void);
extern int LekakBeforeObjectivesInit(const MemoriesModHost *, MemoriesMod *);
static const MemoriesModHost *host;
static void *old_init,*old_update,*old_destroy;
static int english;
static void (*old_applied)(int),(*old_reset)(void);
static struct DuelEffectChannel *boxes[4];
static int selected,opened,page,label=-1;
static u16 visibility[11];
static int hidden,update_logged;

static void remove_box(int i) {
    if(boxes[i]) { TextBox_Destroy(boxes[i]); boxes[i]=0; }
}
static void box(int i,int id,int x,int y,int w,int h) {
    remove_box(i);
    if(!Text_Overridden(id)) return;
    boxes[i]=TextBox_CreateFlagged(i,id,x,y,w,h,0x1028);
    if(boxes[i]) func_80039A60(boxes[i]);
}
static void menu_visibility(int hide) {
    int i;
    for(i=0;i<11;i++) {
        DisplayObject *o=(DisplayObject *)gMain_apMenuEntries[i];
        if(!o) continue;
        if(hide) {
            if(!hidden) visibility[i]=o->flags & DISPLAY_OBJECT_FLAG_RENDERABLE;
            o->flags &= ~DISPLAY_OBJECT_FLAG_RENDERABLE;
        } else if(hidden) {
            o->flags=(o->flags & ~DISPLAY_OBJECT_FLAG_RENDERABLE)|visibility[i];
        }
    }
    hidden=hide;
}
static void close_book(void) {
    int i;
    for(i=0;i<4;i++) remove_box(i);
    label=-1;
    if(hidden) menu_visibility(0);
    opened=0;
}
static void cleanup(void) {
    close_book();remove_box(3);label=-1;selected=0;LekakButton_Restore();
}
static int stable(void) {
    return gMain_bMenuID>=5 && gMain_bMenuID<=10 && !D_80184599 && !D_80184598
        && !D_8018459A && !D_8018459B && !D_8018459C && !D_8018459D;
}
int LekakObjectivesButtonVisible(void){return stable() && !opened;}
int LekakObjectivesButtonSelected(void){return selected;}
static void variant(int id,int active) {
    if(gMain_apMenuEntries[id]) DisplayObject_SetResourceVariant(
        (DisplayObjectConfig *)gMain_apMenuEntries[id],id*2+!active);
}
#include "objectives-progress.h"
static void show_page(void) {
    int id=host->duelist_id(host,objectives[page].identity);
    int unlocked=id>0 && Duelists_Unlocked(gDuel_awPlayerDeck,id);
    int body=0xFE80+page+english*40;
    int heading=0xFED0+page;
    int status=(english ? 0xFE68 : 0xFEFE)+unlocked;
    int footer=0xFEFC+english;
    if(!Text_Overridden(body) || !Text_Overridden(heading)
       || !Text_Overridden(status) || !Text_Overridden(footer)) return;
    remove_box(3);label=-1;
    menu_visibility(1);
    box(1,heading,16,8,288,24);
    box(3,status,16,36,288,16);
    progress_box(body);
    box(2,footer,16,204,288,30);
    opened=1;
}
static void init(s32 unused,s32 menu) {
    cleanup();((void (*)(s32,s32))old_init)(unused,menu);
}
static void destroy(void) {
    cleanup();((void (*)(void))old_destroy)();
}
static s32 update(void) {
    u16 pressed=gInput_wPad1Pressed,held=gInput_wPad1Held,repeat=gInput_wPad1Repeat;
    int consume=0,result;
    u16 navigation=repeat|pressed;
    if(!update_logged && host->log){host->log(host,"Objectives menu update active");update_logged=1;}
    if(opened) {
        consume=1;
        if(pressed&PAD_BUTTON_CANCEL) close_book();
        else if(pressed&PAD_BUTTON_TRIANGLE) {
            english=!english;
            if(host->set_setting) host->set_setting(host,"objectives_language",english);
            show_page();
        }
        else if(pressed&(PAD_BUTTON_R1|PAD_DIRECTION_RIGHT)) {page=(page+1)%40;show_page();}
        else if(pressed&(PAD_BUTTON_L1|PAD_DIRECTION_LEFT)) {page=(page+39)%40;show_page();}
    } else if(stable()) {
        if(selected) {
            if(navigation&PAD_DIRECTION_UP) {selected=0;variant(8,1);consume=1;}
            else if(navigation&PAD_DIRECTION_DOWN) {selected=0;gMain_bMenuID=9;variant(9,1);consume=1;}
            else if(pressed&PAD_BUTTON_CANCEL) selected=0;
            else if(pressed&(PAD_BUTTON_CONFIRM_MASK|PAD_BUTTON_START)) {page=0;show_page();consume=1;}
        } else if((gMain_bMenuID==8 && (navigation&PAD_DIRECTION_DOWN))
               || (gMain_bMenuID==9 && (navigation&PAD_DIRECTION_UP))) {
            variant(gMain_bMenuID,0);gMain_bMenuID=8;selected=1;consume=1;
        }
    }
    /* Channel 1 is rendered by the frontend. Release the menu label before
       stock dialogs reuse this channel during an action or transition. */
    if(!opened && (!stable() || (!consume &&
       (pressed & (PAD_BUTTON_CANCEL|PAD_BUTTON_CONFIRM_MASK|PAD_BUTTON_START))))) {
        remove_box(1);label=-1;
    }
    if(consume) gInput_wPad1Pressed=gInput_wPad1Held=gInput_wPad1Repeat=0;
    result=((s32 (*)(void))old_update)();
    if(consume) {gInput_wPad1Pressed=pressed;gInput_wPad1Held=held;gInput_wPad1Repeat=repeat;}
    if(result!=-1) cleanup();
    else if(opened) menu_visibility(1);
    else if(stable()) {
        if(selected) variant(8,0);
        label=selected;
    } else {remove_box(1);label=-1;selected=0;}
    return result;
}
static void applied(int on) {if(!on) cleanup();if(old_applied) old_applied(on);}
static void reset(void) {
    /* After a save-state load the old display pointers are stale. */
    int i;for(i=0;i<4;i++) boxes[i]=0;
    selected=opened=hidden=0;label=-1;LekakButton_Reset();
    if(old_reset) old_reset();
}
int MemoriesModInit(const MemoriesModHost *h,MemoriesMod *m) {
    if(h->api<5 || !h->hook || !h->duelist_id) return 0;
    if(!LekakBeforeObjectivesInit(h,m)) return 0;
    host=h;
    english=h->setting ? !!h->setting(h,"objectives_language",0) : 0;
    if(!h->hook(h,(void *)MainMenu_InitFrontendMenu,(void *)init,&old_init)
#ifdef MEMORIES_IOS
       || !h->hook(h,(void *)TitleScreen_Update,(void *)update,&old_update)
#else
       || !h->hook(h,(void *)MainMenu_UpdateFrontendMenu,(void *)update,&old_update)
#endif
       || !h->hook(h,(void *)MainMenu_DestroyFrontendMenu,(void *)destroy,&old_destroy)) return 0;
    if(!LekakButton_Init(h)) return 0;
    if(h->log)h->log(h,"Objectives hooks installed");
    old_applied=m->applied;old_reset=m->reset;m->applied=applied;m->reset=reset;
    return 1;
}
