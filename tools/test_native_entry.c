/* Startup control-flow tests with SDK signatures. Not a device execution test. */
#define _POSIX_C_SOURCE 200809L
#include "native_entry.h"
#include "pc/guest/image.h"
#include "pc/guest/state.h"
#include "pc/platform/platform.h"
#include "pc/platform/game_files.h"
#include "pc/mods/mods.h"
#include "pc/debug/log.h"
#include "pc/text/text.h"
#include "pc/text/entry_layout.h"
#include "pc/cards/cards.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int mode,stage,stopped,shutdowns;
void Log_Init(void){assert(stage++==0);}
int Memories_GuestMap(void){assert(stage++==1);return 0;}
int GameFiles_SelectDisc(const char *p,char *why,size_t n){(void)why;(void)n;assert(!strcmp(p,"/disc.bin"));assert(stage++==2);return 0;}
unsigned char *GameFiles_ReadExecutable(const char *p,size_t *n){(void)p;assert(stage++==3);*n=1;return calloc(1,1);}
int Memories_GuestLoadExeData(const unsigned char *p,size_t n,const char *name){(void)p;(void)name;assert(n==1&&stage++==4);return 0;}
int Memories_ModulesInit(void){assert(stage++==5);return 0;}
int Platform_Open(const char *name){
    (void)name;assert(stage++==6);
    /* An old disabled setting must be overridden before the one-time data
     * preparation, not enabled after Platform_Open has already loaded mods. */
    assert(!strcmp(getenv("MEMORIES_MOD_LEKAKMOD"),"1"));
    assert(!strcmp(getenv("MEMORIES_MODS"),"1"));return 0;
}
int Mods_Count(void){return 1;}
const char *Mods_Id(int m){assert(m==0);return "Lekakmod";}
int Mods_Enabled(int m){assert(m==0);return 1;}
int Mods_Active(int m){assert(m==0);return mode!=1;}
int Mods_Failed(int m){assert(m==0);return mode==2;}
const char *Mods_Status(int m){assert(m==0);return "nonfatal note";}
void Platform_ShowError(const char *t,const char *m){assert(!strcmp(t,"Lekak")&&m);}
void Text_Build(void){assert(stage++==7);}
void Cards_Build(void){assert(stage++==8);}
void Text_SortCards(void){assert(stage++==9);}
void TextEntries_Start(void){assert(stage++==11);}
int Main_Init(void){assert(stage++==12);if(mode==3){Platform_StopTimers();Mods_Shutdown();LekakNative_StopGame();}return 17;}
int Memories_StateRunGame(int (*entry)(void)){assert(stage++==10);return entry();}
void Platform_StopTimers(void){stopped++;}
void Mods_Shutdown(void){shutdowns++;}
int main(int argc,char **argv){
    assert(argc==2);mode=atoi(argv[1]);setenv("MEMORIES_MOD_LEKAKMOD","0",1);
    assert(LekakNative_Run("relative","/program","/user")==-1);
    int result=LekakNative_Run("/disc.bin","/program","/user");
    if(mode==1||mode==2){assert(result==-10&&stage==7&&stopped==0&&shutdowns==0);}
    else{assert(result==(mode==3?0:17)&&stage==13&&stopped==1&&shutdowns==1);}
    assert(LekakNative_Run("/disc.bin","/program","/user")==-2);
    return 0;
}
