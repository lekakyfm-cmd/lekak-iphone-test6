/* Exercise the actual hook during selection, purchase and exit ticks. */
#include <assert.h>
#include <stdio.h>
#include "../SourceMod/Lekak/password-shop.c"
PasswordShopCard D_801A8000[SHOP_STAGING_RECORDS];
u8 gPassword_abDigits[8];u16 D_8016D4DC;u16 gDuel_awPlayerDeck[2048];
static int resolutions,identities,updates,selected=805;static unsigned char quantity;
static const char *selected_identity;
int Cards_Valid(int id){return id>0&&id<=1240;}
const char *Cards_Identity(int id){identities++;return id==selected?selected_identity:"Other:card:1";}
int Cards_Type(int id){(void)id;return 0;}
int Duel_GetBaseCardStat(int id,int stat){(void)id;(void)stat;return 5000;}
unsigned char *Cards_ChestSlot(void *p,int id){(void)p;(void)id;return &quantity;}
int Tables_ChestRoom(void){return 255;}
int LekakDuelistsInit(const MemoriesModHost *h,MemoriesMod *m){(void)h;(void)m;return 1;}
s32 Password_LookupCardID(void){return 0;}
void Password_UpdateShopScreen(void){}
s32 Duel_ChestFull(s32 id){(void)id;return 0;}
s32 Campaign_TestStoryFlag(s32 flag){(void)flag;return 1;}
void Library_UpdateCardUsedFlag(s32 flag){(void)flag;}
static int resolve(const MemoriesModHost *h,const char *key){(void)h;resolutions++;return !strcmp(key,selected_identity)?selected:0;}
static void tick(void){updates++;assert(inside_shop);assert(!shop_flag_test(CAMPAIGN_FLAG_PASSWORD_USED_BASE+selected));shop_flag_update(CAMPAIGN_FLAG_PASSWORD_USED_BASE+selected);assert(!shop_chest_full(selected));}
int main(void){MemoriesModHost h={0};h.card_id=resolve;shop_host=&h;
original_lookup=Password_LookupCardID;original_update=tick;original_flag_test=Campaign_TestStoryFlag;original_flag_update=Library_UpdateCardUsedFlag;original_chest_full=Duel_ChestFull;
for(unsigned i=0;i<SHOP_ENTRY_COUNT;i++){if(shop_entries[i].retail)continue;selected_identity=shop_entries[i].identity;assert(shop_known(selected));}
selected_identity="Other:card:1";/* identity is genuine but absent from the mod catalogue */
selected_identity="Other:unknown:1";assert(!shop_known(selected));
for(unsigned i=0;i<SHOP_ENTRY_COUNT;i++)if(shop_entries[i].password==2)selected_identity=shop_entries[i].identity;
gPassword_abDigits[7]=2;assert(shop_lookup()==selected);assert(resolutions==1);resolutions=identities=0;
D_8016D4DC=selected;D_801A8000[selected].price=123;D_801A8000[selected].password=456;
for(int i=0;i<600;i++){shop_update();assert(!inside_shop);assert(D_801A8000[selected].price==123&&D_801A8000[selected].password==456);}
assert(updates==600&&resolutions==0&&identities==2400);
quantity=255;inside_shop=1;assert(shop_chest_full(selected));inside_shop=0;
puts("600 shop/exit ticks: zero identity resolutions; price restoration, repeated purchases and trunk limit passed.");return 0;}
