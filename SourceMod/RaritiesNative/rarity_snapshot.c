#include "rarity_snapshot.h"
#include <stdint.h>

/* The game stores card IDs in signed 15-bit fields. Array bounds are
 * deliberately tighter than 0x7fff (the sentinel is not a card). */
typedef struct {
    uint16_t ids[5][LEKAK_RARITY_TOTAL_BASE_CARDS + 1];
    uint16_t original[LEKAK_RARITY_MAX_CARD_ID + 1];
    uint8_t rarity[LEKAK_RARITY_MAX_CARD_ID + 1];
    unsigned char ready;
} Snapshot;

/* Never expose a partially populated snapshot. The temporary image is
 * filled privately, then published only after all uniqueness checks pass. */
static Snapshot current;
static Snapshot staging;
static unsigned generation;

void LekakRarity_SnapshotClear(void) { ++generation; __builtin_memset(&current, 0, sizeof(current)); }
unsigned LekakRarity_SnapshotGeneration(void) { return generation; }
int LekakRarity_SnapshotReady(void) { return current.ready == 1; }

int LekakRarity_SnapshotBuild(int (*find)(const char *), int total) {
    char identity[40];
    int variants = 0;
    LekakRarity_SnapshotClear();
    __builtin_memset(&staging, 0, sizeof(staging));
    if (!find || total < LEKAK_RARITY_TOTAL_BASE_CARDS ||
        total > LEKAK_RARITY_MAX_CARD_ID) return -1;
    for (int base = 1; base <= LEKAK_RARITY_TOTAL_BASE_CARDS; ++base) {
        const char *key = LekakRarity_BaseIdentity(base);
        int id = key ? find(key) : base;
        if (id < 1 || id > total || staging.original[id]) return -1;
        staging.ids[0][base] = (uint16_t)id;
        staging.original[id] = (uint16_t)base;
    }
    for (int tier = 1; tier <= 4; ++tier) {
        for (int base = 1; base <= LEKAK_RARITY_TOTAL_BASE_CARDS; ++base) {
            if (!LekakRarity_VariantIdentity(base, tier, identity, sizeof(identity)))
                return -1;
            int id = find(identity);
            if (id <= 0) continue; /* collect absent count for all-or-nothing gate */
            if (id > total || staging.original[id]) return -1;
            staging.ids[tier][base] = (uint16_t)id;
            staging.original[id] = (uint16_t)base;
            staging.rarity[id] = (uint8_t)tier;
            ++variants;
        }
    }
    if (!variants) return 0;   /* Only main Lekak is installed. */
    if (variants != LEKAK_RARITY_VARIANT_CARDS) return -1;
    staging.ready = 1;
    __builtin_memcpy(&current, &staging, sizeof(current));
    return 1;
}

/* The manifest's identity grammar is fixed by PC v0.7.6. A zero result
 * indicates another mod's identity; -1 is a malformed rarity identity. */
static int parse_variant(const char *key, int *base, int *tier) {
    static const char prefix[] = "lekak-rarities-test:t";
    if (!key) return 0;
    for (int i = 0; prefix[i]; ++i) {
        if (key[i] != prefix[i]) return 0;
    }
    /* Read no bytes past a truncated NUL-terminated identity. */
    if (__builtin_strlen(key) != sizeof(prefix) - 1 + 9) return -1;
    const char *p = key + sizeof(prefix) - 1;
    if (p[0] < '1' || p[0] > '4' || p[1] != '-' || p[2] != 'b')
        return -1;
    int number = 0;
    for (int i = 3; i < 7; ++i) {
        if (p[i] < '0' || p[i] > '9') return -1;
        number = number * 10 + (p[i] - '0');
    }
    if (p[7] != ':' || p[8] != '1' || p[9] != 0 ||
        number < 1 || number > LEKAK_RARITY_TOTAL_BASE_CARDS) return -1;
    *base = number; *tier = p[0] - '0';
    return 1;
}
int LekakRarity_SnapshotBuildIndexed(int (*find)(const char *),
    const char *(*at)(int), int total) {
    int variants = 0;
    LekakRarity_SnapshotClear();
    __builtin_memset(&staging, 0, sizeof(staging));
    if (!find || !at || total < LEKAK_RARITY_TOTAL_BASE_CARDS ||
        total > LEKAK_RARITY_MAX_CARD_ID) return -1;
    for (int base = 1; base <= LEKAK_RARITY_TOTAL_BASE_CARDS; ++base) {
        const char *key = LekakRarity_BaseIdentity(base);
        int id = key ? find(key) : base;
        if (id < 1 || id > total || staging.original[id]) return -1;
        staging.ids[0][base] = (uint16_t)id;
        staging.original[id] = (uint16_t)base;
    }
    for (int id = 1; id <= total; ++id) {
        int base, tier;
        int match = parse_variant(at(id), &base, &tier);
        if (match < 0) return -1;
        if (!match) continue;
        if (staging.original[id] || staging.ids[tier][base]) return -1;
        staging.ids[tier][base] = (uint16_t)id;
        staging.original[id] = (uint16_t)base;
        staging.rarity[id] = (uint8_t)tier;
        ++variants;
    }
    if (!variants) return 0;
    if (variants != LEKAK_RARITY_VARIANT_CARDS) return -1;
    staging.ready = 1;
    __builtin_memcpy(&current, &staging, sizeof(current));
    return 1;
}
int LekakRarity_SnapshotCardId(int base, int tier) {
    if (!current.ready || base < 1 || base > LEKAK_RARITY_TOTAL_BASE_CARDS ||
        tier < 0 || tier > 4) return -1;
    return current.ids[tier][base];
}
int LekakRarity_SnapshotDecode(int id, int *base, int *tier) {
    if (!current.ready || !base || !tier || id < 1 ||
        id > LEKAK_RARITY_MAX_CARD_ID || !current.original[id]) return 0;
    *base = current.original[id];
    *tier = current.rarity[id];
    return 1;
}
