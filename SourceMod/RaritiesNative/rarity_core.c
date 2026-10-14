#include "rarity_core.h"

static const int bonuses[LEKAK_RARITY_TIERS]={0,100,300,500,800};
static const int resale[LEKAK_RARITY_TIERS]={1,3,8,20,50};
static const uint32_t colours[4][LEKAK_RARITY_KINDS]={
    {0x00C853,0x1976FF,0xFF4F9A,0xFFD400,0xFF8A00},
    {0xE53935,0xFF8A00,0x8E44D4,0x1976FF,0xFFD400},
    {0x00D9ED,0xFFD400,0xEAEFF5,0xFF8A00,0x00C853},
    {0x8E44D4,0xFF4F9A,0xE53935,0x00C853,0x1976FF}
};

int LekakRarity_ChooseTier(int roll){
    if(roll<0||roll>=10000)return -1;
    if(roll<8500)return 0;
    if(roll<9500)return 1;
    if(roll<9800)return 2;
    if(roll<9950)return 3;
    return 4;
}
int LekakRarity_BoostStat(int base,int tier,int cap){
    if(tier<0||tier>=LEKAK_RARITY_TIERS||base<0||cap<0)return -1;
    int64_t value=(int64_t)base+bonuses[tier];
    return (int)(value>cap?cap:value);
}
int LekakRarity_ResaleMultiplier(int tier){
    return tier>=0&&tier<LEKAK_RARITY_TIERS?resale[tier]:-1;
}
int LekakRarity_Slot(int tier,int kind){
    if(tier<1||tier>=LEKAK_RARITY_TIERS||kind<0||kind>=LEKAK_RARITY_KINDS)return -1;
    return (tier-1)*LEKAK_RARITY_KINDS+kind;
}
uint32_t LekakRarity_FrameColor(int tier,int kind){
    if(tier<1||tier>=LEKAK_RARITY_TIERS||kind<0||kind>=LEKAK_RARITY_KINDS)return 0;
    return colours[tier-1][kind];
}
uint16_t LekakRarity_RecolourBGR555(uint16_t w,uint32_t colour){
    int r=w&31,g=(w>>5)&31,b=(w>>10)&31,max=r>g?r:g,min=r<g?r:g;
    int tr=(colour>>19)&31,tg=(colour>>11)&31,tb=(colour>>3)&31;
    int peak=tr>tg?tr:tg,span;
    if(b>max)max=b;if(b<min)min=b;if(tb>peak)peak=tb;
    if(!(w&0x7fff)||max-min<=2||!peak)return w;
    span=max-min;r=min+span*tr/peak;g=min+span*tg/peak;b=min+span*tb/peak;
    return (uint16_t)((w&0x8000)|r|(g<<5)|(b<<10));
}
