#ifndef LEKAK_RARITY_REGISTRY_H
#define LEKAK_RARITY_REGISTRY_H
#include <stddef.h>
#define LEKAK_RARITY_RETAIL_CARDS 722
#define LEKAK_RARITY_TOTAL_BASE_CARDS 1240
#define LEKAK_RARITY_VARIANT_CARDS 4960
/* Returns a reference to a base identity or NULL for retail cards. */
const char *LekakRarity_BaseIdentity(int base_index);
/* Format a stable PC v0.7.6 variant identity into provided buffer.
 * Returns 1 on success, 0 if index/tier/capacity is invalid. */
int LekakRarity_VariantIdentity(int base_index,int tier,char *out,size_t capacity);
/* Probe existing base cards after Cards_Build; read-only, no hooks. */
int LekakRarity_ProbeBases(int (*lookup)(const char *),int live_count);
/* Checks existence of 4960 variant cards (also read-only) and returns
 * number matched. 0 means variants are not installed; NOT enabled by this. */
int LekakRarity_ProbeVariants(int (*lookup)(const char *));
#endif
