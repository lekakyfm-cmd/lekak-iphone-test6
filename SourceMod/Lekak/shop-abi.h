#ifndef LEKAK_SHOP_ABI_H
#define LEKAK_SHOP_ABI_H
/* Minimal shop-only ABI prefix. The original module's relocatable object
 * verifies api at 0, card_id at 0x4c and hook at 0x50. Other fields are opaque;
 * all duelist and notification code remains the supplied compiled object.
 * This header is not a replacement for the complete engine SDK.
 */
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;
typedef struct MemoriesMod MemoriesMod;
typedef struct MemoriesModHost MemoriesModHost;
struct MemoriesModHost {
    u32 api;
    unsigned char opaque_prefix[72];
    int (*card_id)(const MemoriesModHost *, const char *);
    int (*hook)(const MemoriesModHost *, void *, void *, void **);
};
#if __SIZEOF_POINTER__ == 4
_Static_assert(__builtin_offsetof(MemoriesModHost,card_id)==0x4c,"card resolver ABI");
_Static_assert(__builtin_offsetof(MemoriesModHost,hook)==0x50,"hook ABI");
#endif
#define CARD_COUNT 722
#define CAMPAIGN_FLAG_CLEAR_MODIFIER 0x8000
#define CAMPAIGN_FLAG_PASSWORD_USED_BASE 0x400
int Cards_Valid(int);
int Cards_Type(int);
u8 *Cards_ChestSlot(void *,int);
int Tables_ChestRoom(void);
s32 Duel_GetBaseCardStat(s32,s32);
s32 Campaign_TestStoryFlag(s32);
void Library_UpdateCardUsedFlag(s32);
#endif
