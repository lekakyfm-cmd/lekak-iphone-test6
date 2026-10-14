/* Deterministic proof for the optional iPhone reward selector. */
#include "../SourceMod/RaritiesNative/rarity_drop_bridge.h"
#include "../SourceMod/RaritiesNative/rarity_snapshot.h"
#include "../SourceMod/RaritiesNative/rarity_registry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int offset;
static int find(const char *key) {
    if (!strncmp(key,"Lekakmod:",9))
        for (int b=723;b<=1240;++b)
            if (!strcmp(key,LekakRarity_BaseIdentity(b))) return b;
    for (int t=1;t<=4;++t) for (int b=1;b<=1240;++b) {
        char id[40];
        assert(LekakRarity_VariantIdentity(b,t,id,sizeof(id)));
        if (!strcmp(key,id)) return 1241+(t-1)*1240+((b-1+offset)%1240);
    }
    return 0;
}
typedef struct { int first, second, draws; } Roll;
static int rng(void *ctx) {
    Roll *r=ctx; int n = r->draws++;
    return n ? r->second : r->first;
}
static int fixed_roll(void *ctx) { int *n=ctx; ++*n; return 32767; }
static int id_for(int base,int tier){return LekakRarity_SnapshotCardId(base,tier);}
int main(void) {
    Roll roll={9950,0,0};
    LekakRarity_DropBridgeSetEnabled(1);
    assert(!LekakRarity_DropBridgeEnabled());
    assert(LekakRarity_DropBridgeChoose(1,rng,&roll)==1 && roll.draws==0);
    assert(LekakRarity_SnapshotBuild(find,6200)==1);
    assert(!LekakRarity_DropBridgeEnabled());
    LekakRarity_DropBridgeSetEnabled(1);
    assert(LekakRarity_DropBridgeEnabled());
    assert(LekakRarity_DropBridgeChoose(0,rng,&roll)==0 && roll.draws==0);
    assert(LekakRarity_DropBridgeChoose(6201,rng,&roll)==6201 && roll.draws==0);
    assert(LekakRarity_DropBridgeChoose(id_for(100,2),rng,&roll)==id_for(100,2) && roll.draws==0);
    /* All base/tier identities map to the actual shuffled 6200-card catalogue. */
    const int input_rolls[5]={0,8500,9500,9800,9950};
    for (int base=1;base<=1240;++base) for (int tier=0;tier<=4;++tier) {
        Roll r={input_rolls[tier],0,0};
        assert(LekakRarity_DropBridgeChoose(id_for(base,0),rng,&r)==id_for(base,tier));
        assert(r.draws==1);
    }
    /* Same 15-bit acceptance range and biased-tail rejection as PC source. */
    roll=(Roll){30000,9950,0};
    assert(LekakRarity_DropBridgeChoose(42,rng,&roll)==id_for(42,4) && roll.draws==2);
    roll=(Roll){-1,0,0};
    assert(LekakRarity_DropBridgeChoose(42,rng,&roll)==42 && roll.draws==1);
    roll=(Roll){32768,0,0};
    assert(LekakRarity_DropBridgeChoose(42,rng,&roll)==42 && roll.draws==1);
    int calls=0;
    assert(LekakRarity_DropBridgeChoose(42,fixed_roll,&calls)==42 && calls==128);
    int hits[5]={0,0,0,0,0};
    /* Enumerate all 30000 accepted values, each modulo 10000 occurs three times. */
    for(int value=0;value<30000;++value) {
        Roll r={value,0,0};int out=LekakRarity_DropBridgeChoose(42,rng,&r);
        int base,tier;assert(LekakRarity_SnapshotDecode(out,&base,&tier));
        assert(base==42 && r.draws==1);hits[tier]++;
    }
    assert(hits[0]==25500 && hits[1]==3000 && hits[2]==900 && hits[3]==450 && hits[4]==150);
    offset=327;
    assert(LekakRarity_SnapshotBuild(find,6200)==1);
    assert(!LekakRarity_DropBridgeEnabled());
    LekakRarity_DropBridgeSetEnabled(1);
    roll=(Roll){9950,0,0};
    assert(LekakRarity_DropBridgeChoose(42,rng,&roll)==id_for(42,4));
    LekakRarity_DropBridgeSetEnabled(0);
    roll=(Roll){9950,0,0};
    assert(LekakRarity_DropBridgeChoose(42,rng,&roll)==42 && roll.draws==0);
    LekakRarity_SnapshotClear();
    LekakRarity_DropBridgeSetEnabled(1);
    assert(!LekakRarity_DropBridgeEnabled());
    puts("Optional iOS rarity drops bridge OK: 6200 cards, full distribution, rejection, safety, reordering, default-off");
    return 0;
}
