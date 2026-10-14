#ifndef LEKAK_RARITY_SNAPSHOT_H
#define LEKAK_RARITY_SNAPSHOT_H
/* Safe, read-only bridge from the live card catalogue to PC 0.7.6 tiers.
 * DOES NOT enable the variant manifest, hook duel functions, or alter saves.
 * Populate only after Cards_Build, while the game is not running.
 * The full 4x1240 manifest must be installed for the snapshot to be ready.
 */
#include "rarity_registry.h"

enum { LEKAK_RARITY_MAX_CARD_ID = 32766 };
/* 1 = complete/ready, 0 = no variant cards, -1 = partial/invalid.
 * Clears the previous snapshot on every invocation. */
int LekakRarity_SnapshotBuild(int (*find_identity)(const char *), int live_count);
/* Fast production path: scans Cards_Identity once instead of calling the
 * linear Cards_FindIdentity search for each of 4960 variants. */
int LekakRarity_SnapshotBuildIndexed(int (*find_identity)(const char *),
    const char *(*identity_at)(int), int live_count);
int LekakRarity_SnapshotReady(void);
/* Bumped when catalogue snapshot is reset/rebuilt; invalidates enabled hooks. */
unsigned LekakRarity_SnapshotGeneration(void);
void LekakRarity_SnapshotClear(void);
/* Tier 0 returns base game's actual card id. -1 if not ready/invalid. */
int LekakRarity_SnapshotCardId(int base_index, int tier);
/* Returns 1 for a resolved card. Both outputs must be non-NULL. */
int LekakRarity_SnapshotDecode(int id, int *base_index, int *tier);
#endif
