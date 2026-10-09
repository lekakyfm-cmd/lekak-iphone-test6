#include "../App/bios_events.h"
#include "../App/guest_exec.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
int main(void){
 LekakBiosEvents e={0};unsigned h=LekakEvent_Open(&e,0xf0000009,0x20);
 LekakBiosEvent *p=LekakEvent_Get(&e,h);assert(h==0xf1000000&&p&&!p->ready&&!p->enabled);
 LekakEvent_Deliver(&e,0xf0000009,0x20,1);assert(!p->ready);
 p->enabled=1;LekakEvent_Deliver(&e,0xf0000009,0x40,1);assert(!p->ready);
 LekakEvent_Deliver(&e,0xf0000008,0x20,1);assert(!p->ready);
 LekakEvent_Deliver(&e,0xf0000009,0x20,1);assert(p->ready);
 LekakEvent_Deliver(&e,0xf0000009,0x20,0);assert(!p->ready);
 for(unsigned i=1;i<16;i++)assert(LekakEvent_Open(&e,i,i)==0xf1000000+i);
 assert(LekakEvent_Open(&e,9,9)==0xffffffff);
 assert(!LekakEvent_Get(&e,0xf1000010)&&!LekakEvent_Get(&e,0xf0000000));
 /* Execute real B08/B0C/B07/B0B: no readiness until explicit producer,
  * exact class/spec matching, a ready event consumed once. */
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);
 unsigned code[]={0x03e0b821,0x3c04f000,0x34840009,0x24050020,0x24062000,0x24070000,
  0x24090008,0x0c00002c,0,0x00408021,0x00402021,0x2409000c,0x0c00002c,0,
  0x2409000b,0x0c00002c,0,0x00408821,0x3c04f000,0x34840009,0x24050020,
  0x24090007,0x0c00002c,0,0x02002021,0x2409000b,0x0c00002c,0,0x00409021,
  0x2409000b,0x0c00002c,0,0x00409821,0x02e00008,0};
 for(unsigned i=0;i<sizeof(code)/sizeof(*code);i++)Memories_WriteLE32(m->ram+0x10000+4*i,code[i]);
 LekakExecResult result;assert(LekakExec_Run(m,0x80010000,0,0x801fff00,500,&result));
 assert(result.registers[16]==0xf1000000&&result.registers[17]==0&&result.registers[18]==1&&result.registers[19]==0);
 Memories_WriteLE32(m->ram+0x10010,0x24063000);
 assert(!LekakExec_Run(m,0x80010000,0,0x801fff00,500,&result));assert(strstr(result.reason,"event mode unsupported"));
 /* Actual timer2 edge invokes a registered MIPS callback once; its clobbered
  * registers are restored and the auto-ack prevents an interrupt storm. */
 memset(m,0,sizeof(*m));
 const unsigned timer_code[]={
  0x03e0b821,0x24090018,0x0c00002c,0,0x2414004d,0x3c04f200,0x34840002,0x24050002,0x24061000,0x3c078000,0x34e71800,
  0x24090008,0x0c00002c,0,0x00402021,0x2409000c,0x0c00002c,0,
  0x3c081f80,0x24090040,0xad091074,0xad091128,0x24090018,0xad091124,
  0x3c0a8000,0x8d4b0300,0,0x1160fffd,0,0x02e00008,0};
 const unsigned callback[]={0x3c088000,0x24090001,0xad090300,0x24140063,0x03e00008,0};
 for(unsigned i=0;i<sizeof(timer_code)/4;i++)Memories_WriteLE32(m->ram+0x10000+i*4,timer_code[i]);
 for(unsigned i=0;i<sizeof(callback)/4;i++)Memories_WriteLE32(m->ram+0x1800+i*4,callback[i]);
 int ok=LekakExec_Run(m,0x80010000,0,0x801fff00,1000,&result);
 if(!ok)fprintf(stderr,"timer result: %s pc=%08x detail=%08x events=%u irq=%u/%u\n",result.reason,result.pc,result.detail,result.bios_event_callbacks,result.irq_entries,result.irq_returns);
 assert(ok);
 assert(Memories_ReadLE32(m->ram+0x300)==1&&result.registers[20]==77);
 assert(result.bios_event_callbacks==1&&result.irq_entries==1&&result.irq_returns==1&&!(result.irq_status&0x40));
 free(m);puts("BIOS polling event table: lifecycle, bounded capacity, exact producer matching, single consumption and invalid mode rejection and actual timer callback/context passed");
}
