#define _POSIX_C_SOURCE 200809L
#include "native_entry.h"
#include "pc/guest/image.h"
#include "pc/guest/state.h"
#include "pc/platform/platform.h"
#include "pc/platform/game_files.h"
#include "pc/platform/settings.h"
#include "pc/cards/cards.h"
#include "pc/text/text.h"
#include "pc/text/entry_layout.h"
#include "pc/mods/mods.h"
#include "pc/debug/log.h"
#include <stdlib.h>
#include <stdatomic.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#ifdef MEMORIES_IOS
#include "../SourceMod/RaritiesNative/rarity_registry.h"
#include "../SourceMod/RaritiesNative/rarity_snapshot.h"
#include "../SourceMod/RaritiesNative/rarity_drop_bridge.h"
#endif
static double startup_begin;
static double seconds_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void stage(const char *name){fprintf(stderr,"Lekak timing: %.3f s — %s\n",seconds_now()-startup_begin,name);}
static atomic_int launched;
static jmp_buf exit_boundary;
static int boundary_active;
extern int Main_Init(void);
#ifdef MEMORIES_TRANSLATED
/* GameFiles_ReadExecutable is a translated unit: its allocation belongs
 * to the guest runtime registry even though this host entry is ordinary C. */
extern void GuestRuntime_free(void *);
#endif
void Psx___main(void){}
static int begin(void){stage("game initialization begins");TextEntries_Start();int result=Main_Init();stage("game initialization finished");return result;}
void LekakNative_StopGame(void){
    /* Called only on the game worker after VSync stops timers and mods.
     * Return to the host instead of calling exit and terminating UIKit. */
    if(boundary_active)longjmp(exit_boundary,1);
}
int LekakNative_Run(const char *disc,const char *program,const char *user){
    if(!disc||!program||!user||disc[0]!='/'||program[0]!='/'||user[0]!='/')return -1;
    if(atomic_exchange(&launched,1))return -2;
#ifdef MEMORIES_IOS
    /* Start every session fail-closed; no opt-in may survive a previous run. */
    LekakRarity_DropBridgeSetEnabled(0);
#endif
    if(setenv("MEMORIES_DISC",disc,1)||setenv("MEMORIES_PROGRAM_DIR",program,1)||
       setenv("MEMORIES_USER_DIR",user,1)||setenv("MEMORIES_NO_MONITOR","1",1)||
       setenv("MEMORIES_MODS","1",1)||setenv("MEMORIES_MOD_LEKAKMOD","1",1))return -3;
#ifdef MEMORIES_IOS
    /* A second, explicitly requested iOS IPA can ship a DATA-ONLY catalogue.
     * Never allow persisted settings to silently enable 4960 variants in the
     * stable build. The variants have no duel/graphic hooks on iOS yet. */
    int data_only = getenv("LEKAK_IOS_RARITY_DATA_ONLY") &&
        !strcmp(getenv("LEKAK_IOS_RARITY_DATA_ONLY"), "1");
    if(setenv("MEMORIES_MOD_LEKAK_RARITIES_TEST", data_only ? "1" : "0", 1)) return -11;
#endif
    Log_Init();startup_begin=seconds_now();stage("start");
    fprintf(stderr,"Lekak startup: mapping translated memory\n");
    if(Memories_GuestMap())return -4;
    char why[768];size_t size=0;
    if(GameFiles_SelectDisc(disc,why,sizeof(why))){Platform_ShowError("Disc",why);return -5;}
    fprintf(stderr,"Lekak startup: reading USA executable\n");
    unsigned char *exe=GameFiles_ReadExecutable(disc,&size);
    if(!exe)return -6;
    int result=Memories_GuestLoadExeData(exe,size,disc);
#ifdef MEMORIES_TRANSLATED
    GuestRuntime_free(exe);
#else
    free(exe);
#endif
    fprintf(stderr,"Lekak startup: executable loaded (%zu bytes), read buffer released\n",size);
    stage("executable loaded");
    if(result||Memories_ModulesInit())return -7;
    stage("modules initialized");
    fprintf(stderr,"Lekak startup: loading settings and bundled mod\n");
    if(Platform_Open("Lekak"))return -8;
    stage("settings and mod loaded");
    int lekak=-1;
    for(int i=0;i<Mods_Count();i++) {
        const char *id=Mods_Id(i);
        /* User manifest ID is Lekakmod, retained across all releases. */
        if(id&&!strcmp(id,"Lekakmod"))lekak=i;
    }
    if(lekak<0){Platform_ShowError("Lekak","The bundled Lekak manifest was not found.");return -9;}
    /* Restart-required data mods must be enabled before Mods_Load, not
     * toggled afterwards: a late toggle would run the vanilla game once. */
    if(!Mods_Enabled(lekak)||!Mods_Active(lekak)||Mods_Failed(lekak)){
        Platform_ShowError("Lekak",Mods_Status(lekak));return -10;
    }
    if(Mods_Status(lekak)[0])fprintf(stderr,"Lekak mod notes: %s\n",Mods_Status(lekak));
    fprintf(stderr,"Lekak startup: mod active; building text and cards\n");
    Text_Build();stage("text built");Cards_Build();stage("cards built");
#ifdef MEMORIES_IOS
    int matched_bases=LekakRarity_ProbeBases(Cards_FindIdentity,gCard_nCount);
    int snapshot=LekakRarity_SnapshotBuildIndexed(Cards_FindIdentity,Cards_Identity,gCard_nCount);
    fprintf(stderr,"Lekak rarity registry: %d/1240 base cards; snapshot=%d (1=complete, 0=absent, -1=invalid/partial); gameplay hooks NOT enabled\n",matched_bases,snapshot);
    /* An experimental build must never enter a game with a partial catalogue,
     * whether the mod was missing, invalid, or failed to build. Bail out
     * BEFORE Text_SortCards or any sidecar save is read/written. */
    if(data_only){
        int variant_mod=-1;
        for(int i=0;i<Mods_Count();++i)
            if(!strcmp(Mods_Id(i),"lekak-rarities-test")) variant_mod=i;
        if(variant_mod<0 || !Mods_Enabled(variant_mod) ||
            !Mods_Active(variant_mod) || Mods_Failed(variant_mod) ||
            matched_bases!=LEKAK_RARITY_TOTAL_BASE_CARDS ||
            snapshot!=1 || gCard_nCount!=6200){
            Platform_ShowError("Experimental rarity catalogue",
                "Incomplete or invalid iOS data-only variant catalogue. No saves changed.");
            LekakRarity_SnapshotClear();
            return -12;
        }
        /* An additional IPA has an explicit drops opt-in. The data-only IPA
         * still uses unmodified drop selection. Neither enables graphics. */
        int test_drops = getenv("LEKAK_IOS_RARITY_DROPS") &&
            !strcmp(getenv("LEKAK_IOS_RARITY_DROPS"), "1");
        if(test_drops) LekakRarity_DropBridgeSetEnabled(1);
        fprintf(stderr,"Lekak iOS EXPERIMENTAL: %d card identities; drops=%d; duel palettes NOT installed\n",gCard_nCount,LekakRarity_DropBridgeEnabled());
    }else if(snapshot!=0){
        /* Stable IPA must remain on the base catalogue, even if user settings
         * were copied from a separate experimental IPA. */
        LekakRarity_SnapshotClear();
        Platform_ShowError("Rarity catalogue", "Variants unexpectedly loaded in stable build.");
        return -13;
    }
#endif
    Text_SortCards();stage("card names sorted");
    fprintf(stderr,"Lekak startup: entering game\n");
    boundary_active=1;
    if(setjmp(exit_boundary)==0){
        result=Memories_StateRunGame(begin);
        Platform_StopTimers();Mods_Shutdown();
    }else result=0; /* VSync already stopped timers and mods. */
    boundary_active=0;
    return result;
}
