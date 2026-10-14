#include "rarity_save_bridge.h"
#include "rarity_snapshot.h"

static const char prefix[] = "lekak-rarities-test:";

enum { STABLE_ID_LENGTH = 30 };

int LekakRarity_SaveEncode(int id, char *out, size_t capacity) {
    char variant[40];
    int base, tier;
    if (!out || capacity <= STABLE_ID_LENGTH ||
        !LekakRarity_SnapshotDecode(id, &base, &tier) || tier == 0 ||
        !LekakRarity_VariantIdentity(base, tier, variant, sizeof(variant))) return 0;
    for (int i = 0; i <= STABLE_ID_LENGTH; ++i) out[i] = variant[i];
    return 1;
}

int LekakRarity_SaveResolve(const char *identity, int *id) {
    if (!identity || !id) return -1;
    for (unsigned i=0; i < sizeof(prefix)-1; ++i) {
        if (!identity[i]) return 0;
        if (identity[i] != prefix[i]) return 0;
    }
    if (!LekakRarity_SnapshotReady()) return -1;
    const char *p = identity + sizeof(prefix)-1;
    /* Validate exactly t[1-4]-b[0001-1240]:1 with its NUL terminator. */
    for (int i=0; i<10; ++i) if (p[i] == 0) return -1;
    if (p[10] != 0 || p[0] != 't' || p[1] < '1' || p[1] > '4' ||
        p[2] != '-' || p[3] != 'b' || p[8] != ':' || p[9] != '1') return -1;
    int base = 0;
    for (int n=4; n<=7; ++n) {
        if (p[n] < '0' || p[n] > '9') return -1;
        base = base*10 + (p[n]-'0');
    }
    int mapped = LekakRarity_SnapshotCardId(base, p[1]-'0');
    if (mapped <= 0) return -1;
    *id = mapped;
    return 1;
}
