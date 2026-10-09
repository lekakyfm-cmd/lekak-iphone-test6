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
static int begin(void){TextEntries_Start();return Main_Init();}
void LekakNative_StopGame(void){
    /* Called only on the game worker after VSync stops timers and mods.
     * Return to the host instead of calling exit and terminating UIKit. */
    if(boundary_active)longjmp(exit_boundary,1);
}
int LekakNative_Run(const char *disc,const char *program,const char *user){
    if(!disc||!program||!user||disc[0]!='/'||program[0]!='/'||user[0]!='/')return -1;
    if(atomic_exchange(&launched,1))return -2;
    if(setenv("MEMORIES_DISC",disc,1)||setenv("MEMORIES_PROGRAM_DIR",program,1)||
       setenv("MEMORIES_USER_DIR",user,1)||setenv("MEMORIES_NO_MONITOR","1",1)||
       setenv("MEMORIES_MODS","1",1)||setenv("MEMORIES_MOD_LEKAKMOD","1",1))return -3;
    Log_Init();
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
    if(result||Memories_ModulesInit())return -7;
    fprintf(stderr,"Lekak startup: loading settings and bundled mod\n");
    if(Platform_Open("Lekak"))return -8;
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
    Text_Build();Cards_Build();Text_SortCards();
    fprintf(stderr,"Lekak startup: entering game\n");
    boundary_active=1;
    if(setjmp(exit_boundary)==0){
        result=Memories_StateRunGame(begin);
        Platform_StopTimers();Mods_Shutdown();
    }else result=0; /* VSync already stopped timers and mods. */
    boundary_active=0;
    return result;
}
