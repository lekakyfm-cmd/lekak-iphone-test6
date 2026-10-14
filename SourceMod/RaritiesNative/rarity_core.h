#ifndef LEKAK_RARITY_CORE_H
#define LEKAK_RARITY_CORE_H
/* Portable, side-effect-free rules extracted from the Windows 0.7.6 source.
 * Compiled as native ARM64 code. Does not install game hooks, allocate
 * graphic banks, edit deck state, or enable the rarities mod on its own.
 */
#include <stdint.h>

enum { LEKAK_RARITY_BASE_COUNT = 1240, LEKAK_RARITY_KINDS = 5,
       LEKAK_RARITY_TIERS = 5 };

/* PC's Rarity_ChooseTier: 0=normal, 1=rare, 2=epic,
 * 3=legendary, 4=mythic; -1 means invalid roll. */
int LekakRarity_ChooseTier(int roll);
/* PC's boost_stat: [0,100,300,500,800], subject to the game stat cap. */
int LekakRarity_BoostStat(int base, int tier, int cap);
/* PC's rates: [1,3,8,20,50]. */
int LekakRarity_ResaleMultiplier(int tier);
/* Kind order: monster, magic, trap, equip, ritual. */
int LekakRarity_Slot(int tier, int kind);
uint32_t LekakRarity_FrameColor(int tier, int kind);
/* PS1 BGR555 recolour rule: preserve neutral colors and bit 15. */
uint16_t LekakRarity_RecolourBGR555(uint16_t pixel, uint32_t rgb);
#endif
