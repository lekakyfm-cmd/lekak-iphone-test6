#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* The production helper, with only live save/card APIs mocked. */
static int loaded,chest_on_screen,limit=250,gCard_nCount=6200,deck_marker;
static int *gDuel_awPlayerDeck=&deck_marker;
static unsigned char stock[6201],seen[6201];
int Cheats_SaveLoaded(void){return loaded;}
int Cheats_ChestOnScreen(void){return chest_on_screen;}
int Tables_ChestLimit(void){return limit;}
int Cards_Valid(int id){return id>=1&&id<=gCard_nCount&&id!=4000;}
unsigned char *Cards_ChestSlot(void *save,int id){assert(save==gDuel_awPlayerDeck);return &stock[id];}
void Cards_MarkSeen(int id){seen[id]=1;}
#include "../App/ios_full_unlock.h"
int main(void){
 loaded=1;unsetenv("LEKAK_TEST_UNLOCK_CARDS");lekak_test_cards_frame();assert(!stock[1]&&!seen[1]);
 setenv("LEKAK_TEST_UNLOCK_CARDS","0",1);lekak_test_cards_frame();assert(!stock[1]);
 setenv("LEKAK_TEST_UNLOCK_CARDS","1",1);loaded=0;lekak_test_cards_frame();assert(!stock[1]);
 loaded=1;chest_on_screen=1;lekak_test_cards_frame();assert(!stock[1]);
 chest_on_screen=0;stock[3]=150;lekak_test_cards_frame();
 for(int id=1;id<=6200;id++){
  if(id==4000){assert(!stock[id]&&!seen[id]);continue;}
  assert(stock[id]==(id==3?150:99));assert(seen[id]);
 }
 /* A sale stays sold for this session; next save load can grant again. */
 stock[1]=3;lekak_test_cards_frame();assert(stock[1]==3);
 lekak_test_cards_pending=1;lekak_test_cards_frame();assert(stock[1]==99);
 memset(stock,0,sizeof(stock));lekak_test_cards_pending=1;limit=20;lekak_test_cards_frame();assert(stock[6200]==20);
 memset(stock,0,sizeof(stock));lekak_test_cards_pending=1;unsetenv("LEKAK_TEST_UNLOCK_CARDS");lekak_test_cards_frame();assert(!stock[6200]);
 puts("Full Unlock: all base/variant stock and seen flags, normal-build isolation, save/Chest guards, higher stock, sale and cap passed");
}
