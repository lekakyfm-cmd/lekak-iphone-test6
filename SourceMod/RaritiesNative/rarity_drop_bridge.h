#ifndef LEKAK_RARITY_DROP_BRIDGE_H
#define LEKAK_RARITY_DROP_BRIDGE_H
/* Phase 6: guest-game reward selection -> CPU-only rarity tier mapping.
 * This is experimental and may run ONLY after a complete catalogue snapshot
 * and an explicit opt-in. Stable and data-only builds keep their own drops.
 * No graphics, save, stat, library or palette writes are performed here.
 */
int LekakRarity_DropBridgeEnabled(void);
void LekakRarity_DropBridgeSetEnabled(int enabled);
/* The guest supplies its own 15-bit RNG (rand() & 32767), preserving the
 * game's RNG stream. Returns selected unchanged if unavailable/unsafe.
 * Roll rejection above 29999 avoids 32768->10000 modulo bias, as on PC.
 */
int LekakRarity_DropBridgeChoose(int selected, int (*next15)(void *), void *context);
#endif
