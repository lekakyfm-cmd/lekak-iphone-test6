#ifndef LEKAK_REWARDS_LOGIC_H
#define LEKAK_REWARDS_LOGIC_H
#include <stdint.h>
#include <stddef.h>
#define ROSTER_COUNT 79
#define GROUP_COUNT 14
#define CLAIM_COUNT 512
#define REWARD_MAGIC 0x31524b4cU
/* IDs 0..394: original roster in rewards-data.h, five permanent keys each.
 * IDs 395..408: original group goals. Future rewards append at 409; never
 * insert or reassign existing keys. Any layout expansion needs migration. */
typedef struct __attribute__((packed)) {
 uint32_t magic,duelist_code;
 uint16_t version;
 uint8_t claimed[64],tec[10];
 uint32_t checksum;
} RewardSave;
typedef char reward_save_must_fit_reserved_tail[sizeof(RewardSave)==88?1:-1];
typedef struct {const char *ref,*name;} DuelistSpec;
typedef struct {
 const char *name,*en; uint8_t members[40],count,target;
 unsigned stars; const char *card,*card_name;
} GroupSpec;
static uint32_t reward_checksum(const RewardSave *s){
 const uint8_t *p=(const uint8_t *)s;uint32_t h=2166136261U;
 for(size_t i=0;i<offsetof(RewardSave,checksum);i++)h=(h^p[i])*16777619U;
 return h;
}
static void reward_seal(RewardSave *s){s->checksum=reward_checksum(s);}
static int reward_valid(const RewardSave *s,uint32_t code){
 return s->magic==REWARD_MAGIC && s->version==1 && s->duelist_code==code
  && s->checksum==reward_checksum(s);
}
static int reward_claimed(const RewardSave *s,int key){
 return key>=0 && key<CLAIM_COUNT && (s->claimed[key>>3]&(1U<<(key&7)));
}
static int reward_mark(RewardSave *s,int key){
 if(key<0||key>=CLAIM_COUNT||reward_claimed(s,key))return 0;
 s->claimed[key>>3]|=1U<<(key&7);reward_seal(s);return 1;
}
static int reward_tec(const RewardSave *s,int index){
 return index>=0&&index<ROSTER_COUNT&&(s->tec[index>>3]&(1U<<(index&7)));
}
static void reward_mark_tec(RewardSave *s,int index){
 if(index>=0&&index<ROSTER_COUNT){s->tec[index>>3]|=1U<<(index&7);reward_seal(s);}
}
static int reward_stars_fit(unsigned current,unsigned gain,unsigned cap){
 return current<=cap && gain<=cap-current;
}
#endif
