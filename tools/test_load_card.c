#include "../App/guest_exec.h"
#include "../App/cpu_timing.h"
#include "../App/joy_control.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,im) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((im)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static void put(MemoriesMemory *m,const uint32_t *w,size_t n){for(size_t i=0;i<n;i++)Memories_WriteLE32(m->ram+0x1000+i*4,w[i]);}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;
 for(unsigned op=0x20;op<=0x26;op++){
  assert(LekakCpu_LoadWait(I(op,8,9,0),0x80000500)==6);
  assert(LekakCpu_LoadWait(I(op,8,9,0),0xa0000500)==6);
  assert(LekakCpu_LoadWait(I(op,8,9,0),0x1f800000)==0);
  assert(LekakCpu_LoadWait(I(op,8,9,0),0x1f801100)==0);
 }
 assert(LekakCpu_LoadWait(I(0x32,8,9,0),0x80000500)==6);
 assert(LekakCpu_LoadWait(I(0x2b,8,9,0),0x80000500)==0);
 const uint32_t loads[]={I(15,0,8,0x8000),I(0x24,8,9,0x500),I(0x21,8,10,0x500),I(0x23,8,11,0x500),
  I(0x22,8,12,0x501),I(0x26,8,12,0x500),0,RET,0};
 put(m,loads,9);Memories_WriteLE32(m->ram+0x500,0x12345678);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,9,&r)&&r.steps==9&&r.nominal_cycles==39);
 assert(r.registers[9]==0x78&&r.registers[10]==0x5678&&r.registers[11]==0x12345678&&r.registers[12]==0x12345678);
 Memories_WriteLE32(m->ram+0x1000,I(15,0,8,0x1f80));Memories_WriteLE32(m->scratchpad+0x100,0x12345678);
 for(unsigned i=1;i<=5;i++)Memories_WriteLE32(m->ram+0x1000+i*4,loads[i]-0x400);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,9,&r)&&r.nominal_cycles==9&&r.registers[11]==0x12345678);
 const uint32_t joy[]={I(15,0,8,0x1f80),I(0x29,8,0,0x104a),I(0x25,8,9,0x104a),0,RET,0};put(m,joy,6);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&r.joy_control_writes==1&&r.joy_control_reads==1&&r.registers[9]==0);
 const uint32_t badjoy[]={I(15,0,8,0x1f80),I(9,0,9,1),I(0x29,8,9,0x104a)};put(m,badjoy,3);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&strstr(r.reason,"JOY serial")&&r.joy_control_writes==0);
 LekakJoyControl j={0};uint32_t v=123;
 assert(!LekakJoy_Read(&j,0x1f801044,4,&v)&&v==123);
 assert(!LekakJoy_Write(&j,0x1f80104a,4,0)&&!j.writes);
 assert(LekakJoy_Write(&j,0xbf80104a,2,0)==1&&j.writes==1);
 const uint32_t init[]={R(31,0,16,0x21),I(9,0,2,42),I(9,0,4,0),I(9,0,9,0x4a),I(9,0,8,0xb0),
  R(8,0,31,9),0,R(16,0,31,0x21),RET,0};put(m,init,10);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&r.card_initialized&&!r.card_pad_shared&&!r.pad_poll_enabled&&r.registers[2]==42);
 Memories_WriteLE32(m->ram+0x1008,I(9,0,4,1));
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&r.card_pad_shared&&r.pad_poll_enabled&&r.registers[2]==42);
 Memories_WriteLE32(m->ram+0x1008,I(9,0,4,2));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&!r.card_initialized&&strstr(r.reason,"sharing"));
 put(m,init,10);Memories_WriteLE32(m->ram+0x100c,I(9,0,9,0x4b));
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,20,&r)&&r.detail==0x4b&&!r.card_initialized);
 free(m);puts("RAM load stalls separated from budget/fetches, scratchpad timing, load values/merge, disabled JOY control and serial rejection, card idle sharing/void result/invalid inputs and start-before-init rejection passed");return 0;
}
