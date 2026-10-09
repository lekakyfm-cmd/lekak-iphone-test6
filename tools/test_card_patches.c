#include "../App/guest_exec.h"
#include "../App/bios_patches.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,im) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((im)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static const uint32_t early[]={0x8c420018,0,0x8c430070,0,0x3069ffff,0x00094c00,0x8c430074,0,
 0x306affff,0x012a1821,0x24620028,0x3c0a8000,0x254a3000,0x3c098000,0x25293014,
 0x8d430000,0,0xac430000,0x254a0004,0x1549fffb,0x24420004,0x3c010001,0x0c000980,0xac22dffc};
static const uint32_t early_data[]={0x3c02a001,0x2442dfac,0x00400008,0,0};
static const uint32_t early_cont[]={0x8c621074,0,0x30420080,0x1040000b,0,0x8c621044,0,
 0x30420080,0x1440fffc,0,0x3c020001,0x8c42dffc,0,0x00400008,0,0x03e00008,0};
static const uint32_t delay[]={0x8c42016c,0,0x8c4309c8,0x3c0a8000,0x254a3000,0x3c098000,0x25293014,
 0x8d480000,0,0xac4809c8,0x254a0004,0x1549fffb,0x24420004,0x0c000980,0};
static const uint32_t delay_data[]={0x3c08a001,0x2508df80,0x0100f809,0,0};
static const uint32_t delay_cont[]={0x946f000a,0x3c080000,0x01e2c025,0x37190012,0xa479000a,
 0x24080028,0x2508ffff,0x1500fffe,0,0x03e00008,0};
static const uint32_t info[]={0x240a0009,0x8c42016c,0,0x20431988,0x0c000980,0xac600000};
static void put(MemoriesMemory *m,unsigned at,const uint32_t *w,unsigned n){for(unsigned i=0;i<n;i++)Memories_WriteLE32(m->ram+at+i*4,w[i]);}
static void setup(MemoriesMemory *m,unsigned fn){
 memset(m,0,sizeof(*m));
 const uint32_t enter[]={R(31,0,16,0x21),I(9,0,4,0),I(9,0,8,0xb0),I(9,0,9,0x4a),R(8,0,31,9),0,
  I(9,0,9,fn),R(8,0,31,9),0};put(m,0x1000,enter,9);
 const uint32_t flush[]={0x240a00a0,0x01400008,0x24090044};put(m,0x2600,flush,3);
}
static void leave(MemoriesMemory *m,unsigned at){const uint32_t end[]={R(16,0,31,0x21),RET,0};put(m,at,end,3);}
typedef uint32_t (*Patch)(MemoriesMemory *,uint32_t);
static void reject_mutations(MemoriesMemory *m,Patch fn,unsigned at,unsigned words){
 for(unsigned i=0;i<words;i++){
  uint32_t old=Memories_ReadLE32(m->ram+at+i*4);Memories_WriteLE32(m->ram+at+i*4,old^0x10000);
  assert(fn(m,0x80001024)==0);Memories_WriteLE32(m->ram+at+i*4,old);
 }
}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 setup(m,0x56);put(m,0x1024,early,24);put(m,0x3000,early_data,5);put(m,0xdfac,early_cont,17);leave(m,0x1084);
 assert(LekakBios_CardEarlyPatch(m,0x80001024)==0x80001078);
 reject_mutations(m,LekakBios_CardEarlyPatch,0x1024,24);reject_mutations(m,LekakBios_CardEarlyPatch,0x3000,5);
 reject_mutations(m,LekakBios_CardEarlyPatch,0xdfac,17);reject_mutations(m,LekakBios_CardEarlyPatch,0x2600,3);
 assert(!LekakBios_CardEarlyPatch(m,0x801ffffc));
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_card_early_patches==1);
 assert(Memories_ReadLE32(m->ram+0xdffc)==LEKAK_CARD_IRQ_TAIL&&Memories_ReadLE32(m->ram+0x18)==0);
 assert(!LekakExec_Run(m,LEKAK_CARD_IRQ_TAIL,0,0x801fff00,10,&r)&&strstr(r.reason,"serial IRQ tail"));
 /* Recognizing a patch never replaces a missing InitCARD lifecycle. */
 Memories_WriteLE32(m->ram+0x100c,I(9,0,9,0x44));Memories_WriteLE32(m->ram+0x1008,I(9,0,8,0xa0));
 Memories_WriteLE32(m->ram+0x1018,I(9,0,9,0x56));Memories_WriteLE32(m->ram+0x1014,I(9,0,8,0xb0));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"before InitCARD"));
 setup(m,0x57);put(m,0x1024,delay,15);put(m,0x3000,delay_data,5);put(m,0xdf80,delay_cont,11);leave(m,0x1060);
 assert(LekakBios_CardDelayPatch(m,0x80001024)==0x80001058);
 reject_mutations(m,LekakBios_CardDelayPatch,0x1024,15);reject_mutations(m,LekakBios_CardDelayPatch,0x3000,5);
 reject_mutations(m,LekakBios_CardDelayPatch,0xdf80,11);reject_mutations(m,LekakBios_CardDelayPatch,0x2600,3);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_card_delay_patches==1);
 setup(m,0x57);put(m,0x1024,info,6);leave(m,0x103c);
 assert(LekakBios_CardInfoPatch(m,0x80001024)==0x8000103c);
 reject_mutations(m,LekakBios_CardInfoPatch,0x1024,6);reject_mutations(m,LekakBios_CardInfoPatch,0x2600,3);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.bios_card_info_patches==1&&Memories_ReadLE32(m->ram+0x16c)==0);
 /* Lifecycle + backup-driver registry, preserving v0 and refusing bad order. */
 setup(m,0x4b);const uint32_t end[]={I(9,0,8,0xa0),I(9,0,9,0x70),R(8,0,31,9),0,
  R(16,0,31,0x21),RET,0};put(m,0x1024,end,7);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&r.card_running&&r.bu_driver_registered&&(r.irq_mask&0x81)==0x81);
 const uint32_t stop[]={I(9,0,9,0x4c),R(8,0,31,9),0,R(16,0,31,0x21),RET,0};
 setup(m,0x4b);put(m,0x1024,stop,6);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&!r.card_running);
 setup(m,0x4b);Memories_WriteLE32(m->ram+0x1004,I(9,0,4,1));leave(m,0x1024);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"before InitPAD"));
 setup(m,0x4a);put(m,0x1024,end,7);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,100,&r)&&strstr(r.reason,"before StartCARD")&&!r.bu_driver_registered);
 free(m);puts("Card patch whole-body/data/continuation/cache-target mutation rejection, real caller resumption, explicit unsupported tail, init/start/stop and backup registry order passed");return 0;
}
