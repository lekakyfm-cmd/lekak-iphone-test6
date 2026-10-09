#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "game/duel_get_base_card_stat.h"
#include "game/campaign_flags.h"
#include "game/duel_rewards.h"
#include "types.h"
#include <string.h>
/* Password shop declarations without unrelated 32-bit UI layout structs. */
typedef struct { u32 price; s32 password; } PasswordShopCard;
extern PasswordShopCard D_801A8000[];
extern u8 gPassword_abDigits[];
extern u16 D_8016D4DC;
s32 Password_LookupCardID(void);
void Password_UpdateShopScreen(void);
s32 Duel_ChestFull(s32 card_id);

extern int LekakDuelistsInit(const MemoriesModHost *, MemoriesMod *);
extern u16 gDuel_awPlayerDeck[];
typedef struct { const char *identity; unsigned password; int retail; } ShopEntry;
#include "catalogue.h"
static const MemoriesModHost *shop_host;
static s32 (*original_lookup)(void);
static void (*original_update)(void);
static s32 (*original_flag_test)(s32);
static void (*original_flag_update)(s32);
static s32 (*original_chest_full)(s32);
static int inside_shop;

/* The native shop's numeric layout ends at its digit text buffer. Never
 * borrow space beyond it; always restore the selected eight-byte record. */
#define SHOP_STAGING_RECORDS ((0x801B1245u - 0x801A8000u)/sizeof(PasswordShopCard))
static int shop_entry_id(const ShopEntry *entry) {
    return entry->retail ? entry->retail : shop_host->card_id(shop_host, entry->identity);
}
static int shop_known(int id) {
    if (id <= 0 || (unsigned)id >= SHOP_STAGING_RECORDS || !Cards_Valid(id)) return 0;
    /* Identity is indexed by id. Resolving every catalogue entry here made
     * each screen tick perform hundreds of nested card-table scans, including
     * the closing animation. Compare identities without resolving them. */
    const char *identity = id > CARD_COUNT ? Cards_Identity(id) : NULL;
    for (unsigned i=0; i<SHOP_ENTRY_COUNT; i++) {
        const ShopEntry *entry = &shop_entries[i];
        if (entry->retail) {
            if (entry->retail == id) return 1;
        } else if (identity && !strcmp(entry->identity, identity)) return 1;
    }
    return 0;
}
static unsigned shop_price(int attack) {
    unsigned price = 500;
    for (int ceiling=1500; attack>ceiling; ceiling+=500) {
        /* Printed stats are capped at 32767; keep costs inside the shop's
         * six-digit display and the stock 999999-star balance limit. */
        if (price>999999u/2) return 999999;
        price*=2;
    }
    return price;
}
static s32 shop_lookup(void) {
    /* Preserve stock passwords, including edits by other table mods. A
     * duplicate can never silently buy a different card from the stock one. */
    int retail = original_lookup();
    if (retail) return retail;
    unsigned entered = 0;
    for (int i=0; i<8; i++) {
        if (gPassword_abDigits[i]>9) return 0;
        entered=(entered<<4)|gPassword_abDigits[i];
    }
    for (unsigned i=0; i<SHOP_ENTRY_COUNT; i++) {
        if (shop_entries[i].password != entered) continue;
        int id=shop_entry_id(&shop_entries[i]);
        if (id>CARD_COUNT && shop_known(id)) return id;
    }
    return 0;
}
static int shop_purchase_flag(int flag) {
    int id=D_8016D4DC;
    return inside_shop && id>CARD_COUNT && shop_known(id)
        && (flag & ~CAMPAIGN_FLAG_CLEAR_MODIFIER)==CAMPAIGN_FLAG_PASSWORD_USED_BASE+id;
}
static s32 shop_flag_test(s32 flag) {
    /* Added cards have no reserved retail purchase bits. They may be
     * purchased again, up to the trunk limit; never read a duelist's bit. */
    if (shop_purchase_flag(flag)) return 0;
    return original_flag_test(flag);
}
static void shop_flag_update(s32 flag) {
    if (!shop_purchase_flag(flag)) original_flag_update(flag);
}
static s32 shop_chest_full(s32 id) {
    if (inside_shop && id>CARD_COUNT && shop_known(id)) {
        unsigned quantity=*Cards_ChestSlot(gDuel_awPlayerDeck,id);
        if (quantity >= (unsigned)Tables_ChestRoom()) return 1;
    }
    return original_chest_full(id);
}
static void shop_update(void) {
    int id=D_8016D4DC;
    int patch=(id>0 && id<=CARD_COUNT && Cards_Valid(id)) || shop_known(id);
    PasswordShopCard saved;
    if (patch) {
        saved=D_801A8000[id];
        /* Magic, traps, equips and rituals have no ATK: first price tier. */
        int attack=Cards_Type(id)<20 ? Duel_GetBaseCardStat(id,0) : 0;
        D_801A8000[id].price=shop_price(attack);
    }
    int previous=inside_shop;
    inside_shop=1;
    original_update();
    inside_shop=previous;
    if (patch) D_801A8000[id]=saved;
}
int MemoriesModInit(const MemoriesModHost *host, MemoriesMod *mod) {
    if (host->api<5 || !host->hook || !host->card_id) return 0;
    if (!LekakDuelistsInit(host,mod)) return 0;
    shop_host=host;
    if (!host->hook(host,Password_LookupCardID,shop_lookup,(void **)&original_lookup)
        || !host->hook(host,Password_UpdateShopScreen,shop_update,(void **)&original_update)
        || !host->hook(host,Campaign_TestStoryFlag,shop_flag_test,(void **)&original_flag_test)
        || !host->hook(host,Library_UpdateCardUsedFlag,shop_flag_update,(void **)&original_flag_update)
        || !host->hook(host,Duel_ChestFull,shop_chest_full,(void **)&original_chest_full)) return 0;
    return 1;
}
