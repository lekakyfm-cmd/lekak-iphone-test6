#include "pc/mods/modapi.h"
#include "pc/free_duel/duelists.h"
#include "pc/cards/cards.h"
#include "types.h"
/* Live SaveDataState prefix, declared in game/save_data.h. */
extern u16 gDuel_awPlayerDeck[];
#include "game/campaign_flags.h"

/* Existing notification/Kaiba code is linked unchanged, with its entry renamed. */
extern int LekakLegacyInit(const MemoriesModHost *, MemoriesMod *);
static const MemoriesModHost *triad_host;
static void (*legacy_frame)(void);
static void (*legacy_reset)(void);
static void (*legacy_applied)(int);
static void (*legacy_overlay)(void);
static const char *notice_ids[33] = {"Lekakmod:vorthrax", "Lekakmod:nemerys", "Lekakmod:ignaroth", "Lekakmod:vesper", "Lekakmod:azhur", "Lekakmod:ferron", "Lekakmod:nerex", "Lekakmod:ravok", "Lekakmod:tharn", "Lekakmod:zerak", "Lekakmod:nekris", "Lekakmod:oryx", "Lekakmod:valtor", "Lekakmod:serya", "Lekakmod:drazhen", "Lekakmod:malvek", "Lekakmod:nyss", "Lekakmod:krag", "Lekakmod:velkor", "Lekakmod:ossyr", "Lekakmod:nharos", "Lekakmod:solkar", "Lekakmod:luneth", "Lekakmod:voryn", "Lekakmod:rakhan", "Lekakmod:krozak", "Lekakmod:varnek", "Lekakmod:zhyr", "Lekakmod:skorath", "Lekakmod:vezra", "Lekakmod:lesub", "Lekakmod:yem", "Lekakmod:lekak"};
static const char *notice_messages[33] = {"VORTHRAX UNLOCKED!", "NEMERYS UNLOCKED!", "IGNAROTH UNLOCKED!", "VESPER UNLOCKED!", "AZHUR UNLOCKED!", "FERRON UNLOCKED!", "NEREX UNLOCKED!", "RAVOK UNLOCKED!", "THARN UNLOCKED!", "ZERAK UNLOCKED!", "NEKRIS UNLOCKED!", "ORYX UNLOCKED!", "VALTOR UNLOCKED!", "SERYA UNLOCKED!", "DRAZHEN UNLOCKED!", "MALVEK UNLOCKED!", "NYSS UNLOCKED!", "KRAG UNLOCKED!", "VELKOR UNLOCKED!", "OSSYR UNLOCKED!", "NHAROS UNLOCKED!", "SOLKAR UNLOCKED!", "LUNETH UNLOCKED!", "VORYN UNLOCKED!", "RAKHAN UNLOCKED!", "KROZAK UNLOCKED!", "VARNEK UNLOCKED!", "ZHYR UNLOCKED!", "SKORATH UNLOCKED!", "VEZRA UNLOCKED!", "LESUB UNLOCKED!", "YEM UNLOCKED!", "LEKAK UNLOCKED!"};
static int notice_ready, notice_before[33], notice_queue[64], notice_count;
static int notice_active = -1;
static unsigned notice_save;
static uint64_t notice_until;

static void clear_notices(void) {
    notice_ready = 0; notice_count = 0; notice_active = -1; notice_until = 0;
}
static void notice_loaded(MemoriesModEvent *event) {
    if (event->phase == MEMORIES_AFTER) clear_notices();
}
static void update_notices(void) {
    /* Same save identity used by the original notification object (+0x334). */
    unsigned save = *(unsigned *)((unsigned char *)gDuel_awPlayerDeck + 0x334);
    if (notice_ready && save != notice_save) clear_notices();
    for (int i = 0; i < 33; i++) {
        int id = triad_host->duelist_id(triad_host, notice_ids[i]);
        int available = id > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, id);
        if (notice_ready && available && !notice_before[i] && notice_count < 64)
            notice_queue[notice_count++] = i;
        notice_before[i] = available;
    }
    notice_ready = 1; notice_save = save;
    uint64_t now = triad_host->now_us(triad_host);
    if (notice_active >= 0 && now >= notice_until) notice_active = -1;
    if (notice_active < 0 && notice_count) {
        notice_active = notice_queue[0];
        for (int i = 1; i < notice_count; i++) notice_queue[i-1] = notice_queue[i];
        notice_count--;
        notice_until = now + 4000000;
    }
}
static void triad_overlay(void) {
    if (legacy_overlay) legacy_overlay();
    if (notice_active < 0) return;
    int width, height, scale;
    triad_host->overlay_size(triad_host, &width, &height, &scale);
    /* Exact layout recovered from the original notification object's overlay. */
    scale = scale > 0 ? 2 * scale : 2;
    const char *message = notice_messages[notice_active];
    while (scale > 1 && triad_host->text_width(triad_host, message, scale) + 40 * scale > width)
        scale--;
    int text = triad_host->text_width(triad_host, message, scale);
    int x = (width - text)/2;
    int middle = height/5;
    triad_host->fill(triad_host, x - 20 * scale, middle - 22 * scale,
        text + 40 * scale, 44 * scale, 0x160D22, 235);
    triad_host->fill(triad_host, x - 20 * scale, middle - 22 * scale,
        text + 40 * scale, 2 * scale, 0xB73C54, 255);
    triad_host->draw_text(triad_host, x, middle, message, 0xFFE1A6, scale);
}
#define IGNAROTH_FLAG 1807

static void sync_ignaroth(void) {
    int kaiba = triad_host->duelist_id(triad_host, "Lekakmod:tyrant-kaiba");
    int vorthrax = triad_host->duelist_id(triad_host, "Lekakmod:vorthrax");
    int nemerys = triad_host->duelist_id(triad_host, "Lekakmod:nemerys");
    void *state = gDuel_awPlayerDeck;
    int ready = kaiba > 0 && vorthrax > 0 && nemerys > 0
        && Duelists_Unlocked(state, kaiba)
        && Duelists_Unlocked(state, vorthrax)
        && Duelists_Unlocked(state, nemerys)
        && Duelists_RecordSlot(state, vorthrax)[0] >= 5
        && Duelists_RecordSlot(state, nemerys)[0] >= 5;
    /* Recompute for the active save: never inherit the gate from another slot. */
    Library_UpdateCardUsedFlag(IGNAROTH_FLAG |
        (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
/* Flag 1808 gates the second wave; the manifest adds the 200-total-win condition. */
static void sync_second_wave(void) {
    int ignaroth = triad_host->duelist_id(triad_host, "Lekakmod:ignaroth");
    int ready = ignaroth > 0
        && Duelists_Unlocked(gDuel_awPlayerDeck, ignaroth)
        && Duelists_RecordSlot(gDuel_awPlayerDeck, ignaroth)[0] >= 5;
    Library_UpdateCardUsedFlag(1808 | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
/* Distinct Library unlocks, including mod-added cards; duplicate copies count once. */
static int collection_has_200(void) {
    int distinct = 0;
    for (int id = 1; id <= gCard_nCount; id++) {
        if (Cards_Valid(id) && Cards_Seen(id) && ++distinct >= 200) return 1;
    }
    return 0;
}
static void sync_third_wave(void) {
    static const char *previous[3] = {"Lekakmod:vesper", "Lekakmod:azhur", "Lekakmod:ferron"};
    int collection = collection_has_200();
    for (int i = 0; i < 3; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        int ready = collection && id > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, id);
        Library_UpdateCardUsedFlag((1809+i) | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
    }
}
/* Fourth wave: Tharn unlocked and the matching card discovered in the Library. */
static void sync_fourth_wave(void) {
    int tharn = triad_host->duelist_id(triad_host, "Lekakmod:tharn");
    int ready = tharn > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, tharn);
    int cards[3] = {
        triad_host->card_id(triad_host, "Lekakmod:terrorking-archfiend"),
        triad_host->card_id(triad_host, "Lekakmod:card-54"),
        443
    };
    for (int i = 0; i < 3; i++) {
        int open = ready && cards[i] > 0 && Cards_Valid(cards[i]) && Cards_Seen(cards[i]);
        Library_UpdateCardUsedFlag((1812+i) | (open ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
    }
}
/* Fifth wave: one prior boss unlocked; grouped win records for Serya and Drazhen. */
static void sync_fifth_wave(void) {
    static const char *bosses[3] = {"Lekakmod:zerak", "Lekakmod:nekris", "Lekakmod:oryx"};
    static const char *opponents[2][3] = {
        {"Lekakmod:vesper", "Lekakmod:azhur", "Lekakmod:ferron"},
        {"Lekakmod:nerex", "Lekakmod:ravok", "Lekakmod:tharn"}
    };
    for (int i = 0; i < 3; i++) {
        int boss = triad_host->duelist_id(triad_host, bosses[i]);
        int ready = boss > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, boss);
        if (i > 0) for (int j = 0; j < 3; j++) {
            int id = triad_host->duelist_id(triad_host, opponents[i-1][j]);
            if (id <= 0 || Duelists_RecordSlot(gDuel_awPlayerDeck, id)[0] < 10) ready = 0;
        }
        Library_UpdateCardUsedFlag((1815+i) | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
    }
}
/* Shared circus gate: Drazhen unlocked and Saggi discovered in the Library. */
static void sync_sixth_wave(void) {
    int id = triad_host->duelist_id(triad_host, "Lekakmod:drazhen");
    int ready = id > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, id)
        && Cards_Valid(34) && Cards_Seen(34);
    /* Clear former per-duelist flags too, so older save slots cannot retain them. */
    for (int i = 0; i < 3; i++)
        Library_UpdateCardUsedFlag((1818+i) | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
/* Shared gate: all three circus bosses unlocked, and one win against each retail opponent. */
static void sync_seventh_wave(void) {
    static const char *previous[3] = {"Lekakmod:malvek", "Lekakmod:nyss", "Lekakmod:krag"};
    int ready = 1;
    for (int i = 0; i < 3; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        if (id <= 0 || !Duelists_Unlocked(gDuel_awPlayerDeck, id)) ready = 0;
    }
    /* Slot zero is Deck Build; retail opponents occupy exactly slots 1 through 39. */
    for (int id = 1; id < DUELISTS_RETAIL_COUNT; id++)
        if (Duelists_RecordSlot(gDuel_awPlayerDeck, id)[0] < 1) ready = 0;
    Library_UpdateCardUsedFlag(1821 | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
/* Astral bosses: corresponding predecessor unlocked; manifest requires 500 total wins. */
static void sync_eighth_wave(void) {
    static const char *previous[3] = {"Lekakmod:velkor", "Lekakmod:nharos", "Lekakmod:ossyr"};
    for (int i = 0; i < 3; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        int ready = id > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, id);
        Library_UpdateCardUsedFlag((1822+i) | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
    }
}
/* Predator bosses: predecessor unlocked; manifest requires ten wins against it. */
static void sync_ninth_wave(void) {
    static const char *previous[3] = {"Lekakmod:solkar", "Lekakmod:luneth", "Lekakmod:voryn"};
    for (int i = 0; i < 3; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        int ready = id > 0 && Duelists_Unlocked(gDuel_awPlayerDeck, id);
        Library_UpdateCardUsedFlag((1825+i) | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
    }
}
/* Insect trio: all predator bosses unlocked and twenty wins against each. */
static void sync_tenth_wave(void) {
    static const char *previous[3] = {"Lekakmod:rakhan", "Lekakmod:krozak", "Lekakmod:varnek"};
    int ready = 1;
    for (int i = 0; i < 3; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        if (id <= 0 || !Duelists_Unlocked(gDuel_awPlayerDeck, id)
            || Duelists_RecordSlot(gDuel_awPlayerDeck, id)[0] < 20) ready = 0;
    }
    Library_UpdateCardUsedFlag(1828 | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
/* Supreme trio: all 37 preceding mod duelists unlocked and defeated once. */
static void sync_supreme_wave(void) {
    static const char *previous[37] = {"Lekakmod:dark-simon", "Lekakmod:destroyer-pegasus", "Lekakmod:soul-reaper-bakura", "Lekakmod:eclipse-isis", "Lekakmod:primal-rex", "Lekakmod:heishin-the-eternal", "Lekakmod:tyrant-kaiba", "Lekakmod:vorthrax", "Lekakmod:nemerys", "Lekakmod:ignaroth", "Lekakmod:vesper", "Lekakmod:azhur", "Lekakmod:ferron", "Lekakmod:nerex", "Lekakmod:ravok", "Lekakmod:tharn", "Lekakmod:zerak", "Lekakmod:nekris", "Lekakmod:oryx", "Lekakmod:valtor", "Lekakmod:serya", "Lekakmod:drazhen", "Lekakmod:malvek", "Lekakmod:nyss", "Lekakmod:krag", "Lekakmod:velkor", "Lekakmod:ossyr", "Lekakmod:nharos", "Lekakmod:solkar", "Lekakmod:luneth", "Lekakmod:voryn", "Lekakmod:rakhan", "Lekakmod:krozak", "Lekakmod:varnek", "Lekakmod:zhyr", "Lekakmod:skorath", "Lekakmod:vezra"};
    int ready = 1;
    for (int i = 0; i < 37; i++) {
        int id = triad_host->duelist_id(triad_host, previous[i]);
        if (id <= 0 || !Duelists_Unlocked(gDuel_awPlayerDeck, id)
            || Duelists_RecordSlot(gDuel_awPlayerDeck, id)[0] < 1) ready = 0;
    }
    Library_UpdateCardUsedFlag(1829 | (ready ? 0 : CAMPAIGN_FLAG_CLEAR_MODIFIER));
}
static void triad_frame(void) {
    if (legacy_frame) legacy_frame();
    sync_ignaroth();
    sync_second_wave();
    sync_third_wave(); sync_fourth_wave(); sync_fifth_wave(); sync_sixth_wave(); sync_seventh_wave(); sync_eighth_wave(); sync_ninth_wave(); sync_tenth_wave(); sync_supreme_wave();
    update_notices();
}
static void triad_reset(void) {
    clear_notices();
    if (legacy_reset) legacy_reset();
    sync_ignaroth();
    sync_second_wave();
    sync_third_wave(); sync_fourth_wave(); sync_fifth_wave(); sync_sixth_wave(); sync_seventh_wave(); sync_eighth_wave(); sync_ninth_wave(); sync_tenth_wave(); sync_supreme_wave();
}
static void triad_applied(int on) {
    clear_notices();
    if (legacy_applied) legacy_applied(on);
    if (on) { sync_ignaroth(); sync_second_wave(); sync_third_wave(); sync_fourth_wave(); sync_fifth_wave(); sync_sixth_wave(); sync_seventh_wave(); sync_eighth_wave(); sync_ninth_wave(); sync_tenth_wave(); sync_supreme_wave(); }
}
int LekakDuelistsInit(const MemoriesModHost *host, MemoriesMod *mod) {
    if (host->api < 5 || !host->duelist_id || !host->card_id) return 0;
    if (!LekakLegacyInit(host, mod)) return 0;
    triad_host = host;
    if (!host->subscribe(host, MEMORIES_EVENT_LOAD, -1, notice_loaded)
        || !host->subscribe(host, MEMORIES_EVENT_SLOT_LOAD, -1, notice_loaded)) return 0;
    legacy_frame = mod->frame;
    legacy_reset = mod->reset;
    legacy_applied = mod->applied;
    legacy_overlay = mod->overlay;
    clear_notices();
    mod->frame = triad_frame;
    mod->reset = triad_reset;
    mod->applied = triad_applied;
    mod->overlay = triad_overlay;
    mod->overlay_signature = 0;
    return 1;
}
