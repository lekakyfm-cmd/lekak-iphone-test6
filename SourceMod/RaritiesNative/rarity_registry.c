#include "rarity_registry.h"
#include "rarity_base_keys.h"

const char *LekakRarity_BaseIdentity(int base_index){
    if(base_index<=LEKAK_RARITY_RETAIL_CARDS||base_index>LEKAK_RARITY_TOTAL_BASE_CARDS)return 0;
    return LekakRarity_ExtraBaseKeys[base_index-LEKAK_RARITY_RETAIL_CARDS-1];
}

int LekakRarity_VariantIdentity(int base,int tier,char *out,size_t size){
    static const char prefix[]="lekak-rarities-test:t0-b0000:1";
    if(base<1||base>1240||tier<1||tier>4||!out||size<sizeof(prefix))return 0;
    for(size_t i=0;i<sizeof(prefix);++i)out[i]=prefix[i];
    out[21]=(char)('0'+tier);
    out[24]=(char)('0'+(base/1000)%10);
    out[25]=(char)('0'+(base/100)%10);
    out[26]=(char)('0'+(base/10)%10);
    out[27]=(char)('0'+base%10);
    return 1;
}

int LekakRarity_ProbeBases(int (*lookup)(const char *),int live_count){
    if(!lookup||live_count<LEKAK_RARITY_RETAIL_CARDS)return -1;
    int found=LEKAK_RARITY_RETAIL_CARDS;
    for(int base=723;base<=LEKAK_RARITY_TOTAL_BASE_CARDS;++base){
        int id=lookup(LekakRarity_BaseIdentity(base));
        if(id>0&&id<=live_count)++found;
    }
    return found;
}

int LekakRarity_ProbeVariants(int (*lookup)(const char *)){
    if(!lookup)return -1;
    int found=0;
    char identity[40];
    for(int tier=1;tier<=4;++tier)
        for(int base=1;base<=LEKAK_RARITY_TOTAL_BASE_CARDS;++base){
            if(!LekakRarity_VariantIdentity(base,tier,identity,sizeof(identity)))return -1;
            if(lookup(identity)>0)++found;
        }
    return found;
}
