/* LEKAK iPhone: Windows v1.4 fusion rules, iOS static-module port.
 * No graphics/visibility changes.
 * A fusion result has the weakest input tier. Its printed ATK (including
 * rarity bonus) must be >= both input printed ATKs. No new recipes.
 * On iOS, rarity cards may be data-only; exact card identity lookups provide
 * an independent fallback when the Windows rarity services are absent.
 */
#include "types.h"
#include "pc/mods/modapi.h"
#include "pc/cards/cards.h"
#include "pc/cards/tables.h"
#include "game/duel_card_checks.h"
#include "game/duel_card.h"
#include "game/card_constants.h"
#include <stdio.h>

#define LEKAK_RARITY_BASES 1240
#define LEKAK_FUSION_MAX_CARD_ID 16384
static const MemoriesModHost *fusion_host;
static s32 (*original_check_fusion)(s32, s32);
static int (*rarity_of_service)(int);
static int (*rarity_base_service)(int);
static int services_checked, identities_scanned;
static int variant_cache[5][LEKAK_RARITY_BASES + 1];
static unsigned char tier_by_id[LEKAK_FUSION_MAX_CARD_ID];
static unsigned short base_by_id[LEKAK_FUSION_MAX_CARD_ID];
static const int rarity_bonus[5] = {0, 100, 300, 500, 800};

static int identity_for_tier(int base, int tier, char *out, size_t size)
{
    int written;
    if (base < 1 || base > LEKAK_RARITY_BASES || tier < 1 || tier > 4) return 0;
    written = snprintf(out, size, "lekak-rarities-test:t%d-b%04d:1", tier, base);
    return written > 0 && (size_t)written < size;
}

static void scan_rarity_identities(void)
{
    int tier, base, found = 0;
    char identity[64];
    if (identities_scanned) return;
    identities_scanned = 1;
    /* Called during first fusion after other mods have registered their cards. */
    for (tier = 1; tier <= 4; ++tier) {
        for (base = 1; base <= LEKAK_RARITY_BASES; ++base) {
            int id;
            if (!identity_for_tier(base, tier, identity, sizeof(identity))) continue;
            id = fusion_host->card_id(fusion_host, identity);
            if (id > 0 && id < LEKAK_FUSION_MAX_CARD_ID && Cards_Valid(id)) {
                variant_cache[tier][base] = id;
                tier_by_id[id] = (unsigned char)tier;
                base_by_id[id] = (unsigned short)base;
                ++found;
            } else {
                variant_cache[tier][base] = -1;
            }
        }
    }
    if (fusion_host->log)
        fusion_host->log(fusion_host,
            "LEKAK iOS fusion: %d/4960 variants identified; universal ATK guard enabled", found);
}

static void resolve_rarity_services(void)
{
    if (services_checked) return;
    rarity_of_service = (int (*)(int))fusion_host->find(fusion_host, "lekak-rarities-test:rarity");
    rarity_base_service = (int (*)(int))fusion_host->find(fusion_host, "lekak-rarities-test:resale-base");
    services_checked = 1;
    scan_rarity_identities();
}

static int tier_for_card(int id)
{
    if (!Cards_Valid(id)) return -1;
    if (rarity_of_service && rarity_base_service) {
        int tier = rarity_of_service(id);
        if (tier >= 0 && tier <= 4) return tier;
    }
    if (id >= 0 && id < LEKAK_FUSION_MAX_CARD_ID) return tier_by_id[id];
    return 0;
}

static int original_base_for_card(int id)
{
    if (!Cards_Valid(id)) return 0;
    if (rarity_of_service && rarity_base_service) {
        int base = rarity_base_service(id);
        if (Cards_Valid(base)) return base;
    }
    if (id >= 0 && id < LEKAK_FUSION_MAX_CARD_ID && base_by_id[id])
        return base_by_id[id];
    return id;
}

static int unboosted_attack(int id)
{
    if (!Cards_Valid(id)) return -1;
    return ((unsigned)gDuel_adwCardStats[id - 1] & CARD_STAT_VALUE_MASK) * CARD_STAT_SCALE;
}

static int printed_attack(int id)
{
    int tier, base, atk, cap;
    if (!Cards_Valid(id)) return -1;
    tier = tier_for_card(id);
    if (tier >= 1 && tier <= 4 && Cards_Type(id) < CARD_TYPE_MAGIC) {
        base = original_base_for_card(id);
        atk = unboosted_attack(base);
        if (atk < 0) return -1;
        atk += rarity_bonus[tier];
        cap = Tables_StatCap(0);
        return atk > cap ? cap : atk;
    }
    return unboosted_attack(id);
}

static int card_at_tier(int base, int tier)
{
    if (tier == 0) return base;
    if (tier < 1 || tier > 4 || base < 1 || base > LEKAK_RARITY_BASES) return 0;
    return variant_cache[tier][base] > 0 ? variant_cache[tier][base] : 0;
}

static s32 iphone_fusion_with_minimum_rarity(s32 left, s32 right)
{
    int result = original_check_fusion(left, right);
    int a_tier, b_tier, weakest_tier, base_result;
    int atk_left, atk_right, atk_result;
    if (!Cards_Valid(result) || !Cards_Valid(left) || !Cards_Valid(right)) return result;
    resolve_rarity_services();

    a_tier = tier_for_card(left);
    b_tier = tier_for_card(right);
    if (a_tier < 0 || a_tier > 4 || b_tier < 0 || b_tier > 4) return 0;
    weakest_tier = a_tier < b_tier ? a_tier : b_tier;
    base_result = original_base_for_card(result);
    if (!Cards_Valid(base_result)) return 0;
    result = card_at_tier(base_result, weakest_tier);
    if (!Cards_Valid(result)) return 0;

    atk_left = printed_attack(left);
    atk_right = printed_attack(right);
    atk_result = printed_attack(result);
    if (atk_left < 0 || atk_right < 0 || atk_result < 0) return 0;
    if (atk_result < atk_left || atk_result < atk_right) return 0;
    return result;
}

/* build_lekak_ios_objects.py renames this entry to LekakFusionInit. */
int MemoriesModInit(const MemoriesModHost *from, MemoriesMod *mod)
{
    if (!from || !mod || from->api < 5 || !from->find || !from->hook || !from->card_id) return 0;
    fusion_host = from;
    mod->api = MEMORIES_MOD_API;
    return fusion_host->hook(fusion_host, Duel_CheckFusion,
        iphone_fusion_with_minimum_rarity, (void **)&original_check_fusion) != 0;
}
