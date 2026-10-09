#include "../App/guest_exec.h"
#include "../App/pad_driver.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define I(op,rs,rt,im) (((uint32_t)(op)<<26)|((rs)<<21)|((rt)<<16)|((im)&0xffffu))
#define R(rs,rt,rd,fn) (((rs)<<21)|((rt)<<16)|((rd)<<11)|(fn))
#define RET R(31,0,0,8)
static unsigned cursor;
static void emit(MemoriesMemory *m,uint32_t w){Memories_WriteLE32(m->ram+cursor,w);cursor+=4;}
static void imm(MemoriesMemory *m,unsigned r,uint32_t v){emit(m,I(15,0,r,v>>16));emit(m,I(13,r,r,v));}
static void bios(MemoriesMemory *m,unsigned table,unsigned fn){imm(m,8,table);emit(m,I(9,0,9,fn));emit(m,R(8,0,31,9));emit(m,0);}
static void setup(MemoriesMemory *m){
 memset(m,0,sizeof(*m));cursor=0x1000;emit(m,R(31,0,16,0x21));emit(m,I(9,0,20,77));
 imm(m,4,0x80000300);bios(m,0xb0,0x19);
 Memories_WriteLE32(m->ram+0x300,0x80001800);Memories_WriteLE32(m->ram+0x304,0x801ffe00);
 /* Exit hook acknowledges the IRQ and restores the interrupted context. */
 cursor=0x1800;imm(m,8,0x1f800000);emit(m,I(0x2b,8,0,0x1070));imm(m,8,0x80000000);
 emit(m,I(9,0,9,1));emit(m,I(0x2b,8,9,0x200));emit(m,I(9,0,20,99));
 imm(m,8,0xb0);emit(m,I(9,0,9,0x17));emit(m,R(8,0,0,8));emit(m,0);cursor=0x1030;
}
static void node(MemoriesMemory *m,unsigned at,unsigned priority,uint32_t first,uint32_t second){
 Memories_WriteLE32(m->ram+at+4,second);Memories_WriteLE32(m->ram+at+8,first);
 emit(m,I(9,0,4,priority));imm(m,5,0x80000000|at);bios(m,0xc0,2);
}
static void callback(MemoriesMemory *m,unsigned at,unsigned digit,unsigned result,int second){
 unsigned saved=cursor;cursor=at;imm(m,8,0x80000000);
 if(second)emit(m,I(0x2b,8,4,0x208));
 emit(m,I(0x23,8,10,0x204));emit(m,0);emit(m,I(9,0,11,10));emit(m,R(10,11,0,0x18));
 emit(m,R(0,0,10,0x12));emit(m,I(9,10,10,digit));emit(m,I(0x2b,8,10,0x204));
 emit(m,I(9,0,2,result));emit(m,RET);emit(m,0);cursor=saved;
}
static void timer_and_wait(MemoriesMemory *m){
 imm(m,8,0x1f800000);emit(m,I(9,0,9,0x10));emit(m,I(0x2b,8,9,0x1074));
 emit(m,I(9,0,9,3));emit(m,I(0x2b,8,9,0x1108));emit(m,I(9,0,9,0x18));emit(m,I(0x2b,8,9,0x1104));
 imm(m,10,0x80000000);emit(m,I(0x23,10,11,0x200));emit(m,0);emit(m,I(4,11,0,-3));emit(m,0);
 emit(m,R(16,0,31,0x21));emit(m,RET);emit(m,0);
}
int main(void){
 MemoriesMemory *m=calloc(1,sizeof(*m));assert(m);LekakExecResult r;LekakPadDriver p={0};
 memset(m->ram+0x400,0xaa,0x22);assert(!LekakPad_Init(&p,m,0x80000400,0x22,0x801ffff0,0x22));
 assert(m->ram[0x400]==0xaa&&!p.initialized);assert(!LekakPad_Init(&p,m,0x80000400,8,0x80000480,0x22));
 assert(LekakPad_Init(&p,m,0x80000400,0x22,0x80000480,0x22));
 p.connected[0]=1;p.pressed[0]=0x4010;assert(LekakPad_Poll(&p,m,1)&&!p.polls);
 p.started=1;assert(LekakPad_Poll(&p,m,0)&&!p.polls);assert(LekakPad_Poll(&p,m,1)&&p.polls==1);
 assert(m->ram[0x400]==0&&m->ram[0x401]==0x41&&m->ram[0x402]==0xef&&m->ram[0x403]==0xbf);
 assert(m->ram[0x480]==0xff);p.buffers[1]=0x801ffff0;m->ram[0x400]=0xaa;
 assert(!LekakPad_Poll(&p,m,1)&&m->ram[0x400]==0xaa);
 setup(m);node(m,0x500,0,0x80002000,0x80002100);node(m,0x510,1,0x80002200,0x80002300);
 node(m,0x520,1,0x80002400,0x80002500);
 callback(m,0x2000,1,7,0);callback(m,0x2100,2,0,1);callback(m,0x2200,4,0,0);
 callback(m,0x2300,9,0,1);callback(m,0x2400,3,0,0);callback(m,0x2500,9,0,1);timer_and_wait(m);
 assert(LekakExec_Run(m,0x80001000,0x1234,0x801fff00,1000,&r));
 assert(r.irq_chain_calls==4&&Memories_ReadLE32(m->ram+0x204)==1234&&Memories_ReadLE32(m->ram+0x208)==7);
 assert(r.irq_entries==1&&r.irq_returns==1&&r.registers[20]==77&&r.registers[28]==0x1234&&r.registers[29]==0x801fff00);
 /* An early ReturnFromException must skip all remaining nodes and exit hook. */
 setup(m);node(m,0x500,0,0x80002000,0x80001800);node(m,0x510,1,0x80002200,0);
 callback(m,0x2000,1,1,0);callback(m,0x2200,9,1,0);timer_and_wait(m);
 assert(LekakExec_Run(m,0x80001000,0,0x801fff00,1000,&r)&&r.irq_chain_calls==2&&Memories_ReadLE32(m->ram+0x204)==1);
 /* A callback can corrupt links after registration: dispatch remains bounded. */
 setup(m);node(m,0x500,0,0x80002000,0);unsigned saved=cursor;cursor=0x2000;
 imm(m,8,0x80000500);emit(m,I(0x2b,8,8,0));emit(m,I(9,0,2,0));emit(m,RET);emit(m,0);cursor=saved;timer_and_wait(m);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,10000,&r)&&strstr(r.reason,"dispatch chain"));
 setup(m);node(m,0x500,0,0x1f801000,0);timer_and_wait(m);
 assert(!LekakExec_Run(m,0x80001000,0,0x801fff00,1000,&r)&&strstr(r.reason,"memory"));
 /* Real guest InitPAD/StartPAD + VBlank feeds explicit digital input. */
 setup(m);imm(m,4,0x80000400);emit(m,I(9,0,5,0x22));imm(m,6,0x80000480);emit(m,I(9,0,7,0x22));
 bios(m,0xb0,0x12);bios(m,0xb0,0x13);imm(m,10,0x80000400);
 emit(m,I(0x24,10,11,1));emit(m,0);emit(m,I(4,11,0,-3));emit(m,0);
 bios(m,0xb0,0x14);emit(m,R(16,0,31,0x21));emit(m,RET);emit(m,0);
 const LekakExecPads pads={{1,0},{0x4010,0},0,0,0};
 assert(LekakExec_RunWithPads(m,0x80001000,0,0x801fff00,600000,&r,&pads));
 assert(r.pad_initializations==1&&r.pad_polls==1&&r.pad_initialized&&!r.pad_started);
 assert(m->ram[0x401]==0x41&&m->ram[0x402]==0xef&&m->ram[0x403]==0xbf&&m->ram[0x480]==0xff);
 /* A scheduled press/release changes real active-low receive bytes. */
 LekakPadDriver timed={0};timed.connected[0]=1;timed.press_frame=3;timed.release_frame=5;timed.scheduled[0]=8;
 assert(LekakPad_Init(&timed,m,0x80000400,34,0x80000480,34));timed.started=1;
 LekakPad_InputAt(&timed,2);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xff);
 LekakPad_InputAt(&timed,3);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xf7);
 LekakPad_InputAt(&timed,4);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xf7);
 LekakPad_InputAt(&timed,5);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xff);
 timed.repeat_frames=10;
 LekakPad_InputAt(&timed,13);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xf7);
 LekakPad_InputAt(&timed,15);assert(LekakPad_Poll(&timed,m,1)&&m->ram[0x402]==0xff);
 free(m);puts("Pad validation, atomic bounds, digital bytes, VBlank polling, stop, IRQ priority/order/return gating, early return, corruption bounds and context restoration passed");return 0;
}
