#ifndef LEKAK_RARITY_SAVE_BRIDGE_H
#define LEKAK_RARITY_SAVE_BRIDGE_H
/* Stable, portable identity bridge for future iOS save/collection hooks.
 * It never reads or writes any save file and never mutates the live catalogue.
 * The game already has its own sidecar save implementation; use its identity
 * schema to remap variant ids across future catalogue reordering.
 */
#include <stddef.h>
/* Writes the stable identity of a variant id into out, including terminator.
 * Returns 1 on success, 0 if the snapshot is absent, id isn't a variant, or
 * the destination is too small. Does not modify out on failure. */
int LekakRarity_SaveEncode(int id, char *out, size_t capacity);
/* 1: recognized and resolved, 0: unrelated identity, -1: malformed rarity
 * identity or snapshot absent. The output id is written only on success. */
int LekakRarity_SaveResolve(const char *identity, int *id);
#endif
