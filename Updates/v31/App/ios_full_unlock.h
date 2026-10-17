/* Included by the staged cards.c so the real guest save/card APIs are used.
 * Only the Full Unlock IPA sets LEKAK_TEST_UNLOCK_CARDS=1. Apply once per
 * loaded/new save, outside Build Deck's private copy, and preserve higher stock. */
#include "pc/debug/cheats.h"
static int lekak_test_cards_pending=1;
static void lekak_test_cards_frame(void){
 const char *enabled=getenv("LEKAK_TEST_UNLOCK_CARDS");
 if(!lekak_test_cards_pending||!enabled||strcmp(enabled,"1")||
    !Cheats_SaveLoaded()||Cheats_ChestOnScreen())return;
 int copies=Tables_ChestLimit();if(copies>99)copies=99;if(copies<1)return;
 for(int id=1;id<=gCard_nCount;id++)if(Cards_Valid(id)){
  unsigned char *stock=Cards_ChestSlot(gDuel_awPlayerDeck,id);
  if(*stock<copies)*stock=(unsigned char)copies;
  Cards_MarkSeen(id);
 }
 lekak_test_cards_pending=0;
 fprintf(stderr,"Lekak Full Unlock: all %d cards (including variants) seen, at least %d copies each\n",gCard_nCount,copies);
}
