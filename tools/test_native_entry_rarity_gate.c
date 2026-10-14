/* Tests the iOS-specific catalogue gate with mock game data; not an iPhone test. */
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
#include "pc/guest/translated_runtime.h"
#include "../SourceMod/RaritiesNative/rarity_registry.h"
#include "../SourceMod/RaritiesNative/rarity_drop_bridge.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int mode, seq, errors;
static MemoriesMemory memory;
static char variants[6201][40];
int gCard_nCount;
extern void *GuestRuntime_malloc(size_t);
void Log_Init(void){assert(seq++==0);}
int Memories_GuestMap(void){assert(seq++==1);return GuestRuntime_Bind(&memory);}
int GameFiles_SelectDisc(const char *p,char *why,size_t n){(void)why;(void)n;assert(!strcmp(p,"/disc.bin"));assert(seq++==2);return 0;}
unsigned char *GameFiles_ReadExecutable(const char *p,size_t *n){(void)p;assert(seq++==3);*n=1900544;return GuestRuntime_malloc(*n);}
int Memories_GuestLoadExeData(const unsigned char *p,size_t n,const char *name){(void)p;(void)name;assert(n==1900544&&seq++==4);return 0;}
int Memories_ModulesInit(void){assert(seq++==5);return 0;}
int Platform_Open(const char *name){(void)name;assert(seq++==6);
    assert(!strcmp(getenv("MEMORIES_MOD_LEKAK_RARITIES_TEST"), mode==0?"0":"1"));return 0;}
int Mods_Count(void){return mode==0?1:2;}
const char *Mods_Id(int m){return m==0?"Lekakmod":"lekak-rarities-test";}
int Mods_Enabled(int m){return m==0?1:mode==4?0:1;}
int Mods_Active(int m){return m==0?1:mode==4?0:1;}
int Mods_Failed(int m){(void)m;return 0;}
const char *Mods_Status(int m){(void)m;return "";}
void Platform_ShowError(const char *t,const char *m){assert(t&&m);errors++;}
void Text_Build(void){assert(seq++==7);}
void Cards_Build(void){assert(seq++==8);gCard_nCount=((mode==2||mode==5)?6200:mode==3?6199:1240);
    for(int tier=1;tier<=4;++tier)for(int base=1;base<=1240;++base){
        int id=1240+(tier-1)*1240+base;
        assert(LekakRarity_VariantIdentity(base,tier,variants[id],sizeof variants[id]));
    }
}
const char *Cards_Identity(int id){
    if(id<723)return "";
    if(id<=1240)return LekakRarity_BaseIdentity(id);
    return id<=gCard_nCount?variants[id]:"";
}
int Cards_FindIdentity(const char *key){
    if(!key)return 0;
    for(int i=723;i<=1240;++i)if(!strcmp(key,LekakRarity_BaseIdentity(i)))return i;
    return 0;
}
void Text_SortCards(void){assert(seq++==9);assert(LekakRarity_DropBridgeEnabled()==(mode==5));}
void TextEntries_Start(void){assert(seq++==11);}
int Main_Init(void){assert(seq++==12);return 17;}
int Memories_StateRunGame(int (*entry)(void)){assert(seq++==10);return entry();}
void Platform_StopTimers(void){}
void Mods_Shutdown(void){}
int main(int argc,char **argv){
    assert(argc==2);mode=atoi(argv[1]);
    if(mode)setenv("LEKAK_IOS_RARITY_DATA_ONLY","1",1);
    else unsetenv("LEKAK_IOS_RARITY_DATA_ONLY");
    if(mode==5)setenv("LEKAK_IOS_RARITY_DROPS","1",1);
    else unsetenv("LEKAK_IOS_RARITY_DROPS");
    assert(LekakNative_Run("/disc.bin","/program","/user")==
       ((mode==0||mode==2||mode==5)?17:-12));
    assert(errors==(mode==0||mode==2||mode==5?0:1));
    assert(seq==((mode==0||mode==2||mode==5)?13:9));
    GuestRuntime_Reset();
    return 0;
}
