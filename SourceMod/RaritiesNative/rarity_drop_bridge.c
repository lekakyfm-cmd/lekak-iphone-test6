#include "rarity_drop_bridge.h"
#include "rarity_snapshot.h"
#include "rarity_core.h"

static int enabled;
static unsigned enabled_generation;

void LekakRarity_DropBridgeSetEnabled(int on) {
    enabled = on && LekakRarity_SnapshotReady();
    enabled_generation = enabled ? LekakRarity_SnapshotGeneration() : 0;
}
int LekakRarity_DropBridgeEnabled(void) {
    return enabled && LekakRarity_SnapshotReady() &&
        enabled_generation == LekakRarity_SnapshotGeneration();
}
int LekakRarity_DropBridgeChoose(int selected, int (*next15)(void *), void *context) {
    int base, old_tier, roll, new_tier, id;
    if (!LekakRarity_DropBridgeEnabled() || !next15 ||
        !LekakRarity_SnapshotDecode(selected, &base, &old_tier) || old_tier != 0)
        return selected;
    /* A pathological callback must never stall a game worker forever. */
    for (int attempt = 0; attempt < 128; ++attempt) {
        roll = next15(context);
        if (roll < 0 || roll >= 32768) return selected;
        if (roll >= 30000) continue;
        new_tier = LekakRarity_ChooseTier(roll % 10000);
        if (new_tier < 0) return selected;
        id = LekakRarity_SnapshotCardId(base, new_tier);
        return id > 0 ? id : selected;
    }
    return selected;
}
